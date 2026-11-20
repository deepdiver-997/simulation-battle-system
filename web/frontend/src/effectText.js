// 全量状态翻译层（开工文档 §2.1：协议只传 ID，名字解析全在客户端）。
//
// 两个注册空间**数字重叠**（effect 1004 ≠ 魂印 1004，见常识速查），消费端按
// soul 标志选表：
// - 技能空间 effects[id].info = effect_info 模板，{n} 占位符由 args 代入；
// - 魂印空间 soulEffects[id].tips = effect_icon 描述（纯文本）。
// 技能名（效果桶"来源"列）按需拉 /api/moves/{id}，缓存进响应式对象。
import { reactive } from 'vue'
import { get } from './api'

export const effectIndex = reactive({ loaded: false, effects: {}, soulEffects: {} })

export async function loadEffectIndex(force = false) {
  if (effectIndex.loaded && !force) return effectIndex
  try {
    const data = await get('/effect-index')
    effectIndex.effects = data.effects || {}
    effectIndex.soulEffects = data.soulEffects || {}
    effectIndex.loaded = true
  } catch { /* 网关未起：全量状态退化为裸 id 显示 */ }
  return effectIndex
}

/** effect_info 模板 {n} 占位符代入；args 缺失时保留占位符原样。 */
export function renderTemplate(tpl, args) {
  if (!tpl) return ''
  if (!args || !args.length) return tpl
  return String(tpl).replace(/\{(\d+)\}/g, (m, n) =>
    args[+n] === undefined ? m : String(args[+n]))
}

/** 技能空间效果文案：模板 + args 代入。 */
export function skillEffectText(effectId, args) {
  const e = effectIndex.effects[effectId]
  return e ? renderTemplate(e.info, args) : ''
}

/** 魂印空间效果文案：effect_icon tips（sourceId = 魂印 id）。 */
export function soulEffectText(sourceId) {
  return effectIndex.soulEffects[sourceId]?.tips || ''
}

export const moveNameCache = reactive({})

/** 技能 id → 名字（懒加载，未到达前返回 '' 由调用方显示 `技能#id` 兜底）。 */
export function moveName(id) {
  if (!id || id <= 0) return ''
  if (!(id in moveNameCache)) {
    moveNameCache[id] = null
    get(`/moves/${id}`).then((d) => { moveNameCache[id] = d?.name || null })
      .catch(() => { moveNameCache[id] = null })
  }
  return moveNameCache[id] || ''
}
