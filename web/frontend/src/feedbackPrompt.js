// 反馈提醒（2026-10-23）：纯客户端计数，完成对局达阈值后弹一次反馈引导。
// 只读 localStorage，不碰服务端、不按 IP 识别用户（访客 IP 半敏感口径见
// web/gateway/visit_stats.py——2026-10-06 下线 /api/stats 即此因）。
// 状态机（localStorage "sbs_feedback_prompt"）：
//   {}                       从未问过 → 累计完成 FIRST_ASK_AFTER 局后首问
//   { state:'asked', done, nextAskAt }   问过但"下次再说" → 再完成（nextAskAt-done）局后二问
//   { state:'done' }         点过任一反馈渠道或选"不再提示" → 永不再问
import { reactive } from 'vue'

const KEY = 'sbs_feedback_prompt'
const FIRST_ASK_AFTER = 3   // 首次提醒：累计完成 N 局
const SNOOZE_AFTER = 10     // "下次再说"：再完成 M 局后问下一次

export const feedbackPrompt = reactive({ visible: false, manual: false })

function load() {
  try { return JSON.parse(localStorage.getItem(KEY)) || {} } catch { return {} }
}
function save(v) { localStorage.setItem(KEY, JSON.stringify(v)) }

/** 每完成一局调一次（store.js handleEvent 'battle_over'） */
export function recordBattleOver() {
  if (feedbackPrompt.visible) return   // 弹着的时候不再叠加触发
  const st = load()
  if (st.state === 'done') return
  const done = (st.done || 0) + 1
  if (done < (st.nextAskAt || FIRST_ASK_AFTER)) {
    save({ ...st, done })
    return
  }
  save({ ...st, state: 'asked', done, nextAskAt: done + SNOOZE_AFTER })
  feedbackPrompt.visible = true
}

/** 头部「💬 反馈」按钮：手动打开（不算打扰额度，也不写状态机） */
export function openFeedback() {
  feedbackPrompt.manual = true
  feedbackPrompt.visible = true
}

/** 关闭（= 自动模式下"下次再说"：额度在触发时已记好，无需额外动作） */
export function closeFeedback() {
  feedbackPrompt.visible = false
  feedbackPrompt.manual = false
}

/** "不再提示"/点过任一反馈渠道：永久安静 */
export function dismissFeedbackForever() {
  save({ ...load(), state: 'done' })
  closeFeedback()
}
