import { reactive } from 'vue'
import { BattleSocket, get, postJson } from './api'
import { recordBattleOver } from './feedbackPrompt'

/** 培养配置默认值（培养线 feat/peiyang）。enabled=false = 旧协议语义（直传数值）。 */
export function defaultCultivate() {
  return {
    enabled: false,
    ev: [0, 0, 0, 0, 0, 0],           // 学习力：单项 ≤255、总和 ≤510（引擎校验）
    natureId: -1,                     // nature 表 id；-1 = 中性（全 1.0）
    titleId: 0,                       // 称号 title_stats.id；0 = 无（flat 性格后平加）
    inscriptions: [0, 0, 0],          // 刻印 item_id，0 = 空槽
    hpCap20: true,                    // 体力上限 +20（逐精灵开关，拟真默认开）
    guild: true,                      // 战队加成六维（工作值 30/15×4/10）
    annual: false,                    // 年费通用加成（六维+10 平加，不吃性格）
    titleId: 0,                       // 称号（title_stats.id，0=无；平加 pvp/pve 双有效）
    resist: {                         // 抗性训练（高级段；不进面板）
      enabled: false,
      damage: [0, 0, 0],              // [暴击, 固定, 百分比]，官方钳 35
      control: [],                    // [{anomalyId, pct}] ≤3，官方钳 50
      uncontrol: [],
      completeImmunity: 0,
    },
  }
}

/** 槽位 */
export function emptySlot() {
  return {
    petId: 0, name: '', detail: null, skills: [], traitId: 0,
    base: [0, 0, 0, 0, 0, 0],
    // 神谕开关（神谕波次精灵）：true = 神谕后魂印 + 解锁神谕新技
    oracle: false,
    // 词条参数覆盖（词条编辑线）：[{skill, effect, index, value}]——index = 模板 {n} 下标
    effectParams: [],
    // 效果禁用（2026-09-26 禁用基建）：{ skillEffects: [id...], soulMarks: [id...] }
    disabledEffects: { skillEffects: [], soulMarks: [] },
    // 培养配置（培养线）：见 defaultCultivate()
    cultivate: defaultCultivate(),
  }
}

export const team = reactive({
  mode: 'direct',            // direct=直开 6v6 | bp=BP 赛前快照（带待命背包）
  // boss 挑战开关（2026-09-26 "boss 有效"线）：true = 魂印 boss_invalid 节点不注册
  //（子句级 boss 无效）；技能效果经槽位 disabledEffects 手动/一键预填。
  bossChallenge: false,
  side1: Array.from({ length: 6 }, emptySlot),
  side2: Array.from({ length: 6 }, emptySlot),
  // 药剂库存：每方 { itemId: 数量 }——battle_items 表驱动，空 = 嗑药区无货
  medicines: { side1: {}, side2: {} },
  // 装备/套装（2026-09-28 三视角线）：每方穿戴部件 id 清单——成套数值在引擎
  // resolve_equipment 落本体（flat/pct/pvp/pve 维度），预览走 /cultivation/preview 的 equip 段
  equip: { side1: [], side2: [] },
})

// ── 双背包模型（2026-09-27 用户口径：出战/待命背包都是**战前资产池**）──
// 出战背包 6 格（team.side1/side2）+ 待命背包 6 格（standbySlots）= 每方至多 12 只，
// 两包都完整配置（待命宠 BP 后也可能被选上场）。真正的 SeerRobot::elfPets =
// **BP 选序结果**（对手禁 3 后按挑选顺序从 12 只里选 1~6 只，空位垫底）——
// 出战背包 ≠ elfPets。payload.standby = 未被选入出战的待命宠 id（1551 等赛前读者计数域）。

/** 待命背包：完整 6 格 × 2 方（与出战背包同构） */
export const standbySlots = {
  side1: Array.from({ length: 6 }, emptySlot),
  side2: Array.from({ length: 6 }, emptySlot),
}

/** BP 出战选序：side → [{pack:'battle'|'standby', idx}]（空 = 出战背包非空格按序） */
export const bpOrder = {
  side1: [],
  side2: [],
}

/** BP 禁选：side → [{pack:'battle'|'standby', idx}]（对手禁的 ≤3；构造出场名单时强制排除） */
export const bpBanned = {
  side1: [],
  side2: [],
}

/** 一侧的出场名单。direct 模式：只认出战背包按序（选序/禁选/待命全部不参与）。
 *  bp 模式：选序驱动（可含待命宠），被禁的卡无论在选序还是默认序里都不进名单。 */
export function sideLineup(side) {
  if (team.mode === 'direct') {
    return team[side].filter((s) => s && s.petId > 0).slice(0, 6)
  }
  const order = bpOrder[side] || []
  const banned = bpBanned[side] || []
  const banHit = (pack, idx) => banned.some((b) => b.pack === pack && b.idx === idx)
  if (order.length) {
    return order
      .filter(({ pack, idx }) => !banHit(pack, idx))
      .map(({ pack, idx }) => (pack === 'standby' ? standbySlots[side][idx] : team[side][idx]))
      .filter((s) => s && s.petId > 0)
      .slice(0, 6)
  }
  return team[side]
    .filter((s, idx) => s && s.petId > 0 && !banHit('battle', idx))
    .slice(0, 6)
}

export function lineupPayload() {
  // 单 slot → 协议 pet 对象（培养/覆盖/禁用各可选段；petId<=0 = 空位 → null）
  const build = (s) => {
    if (!s || s.petId <= 0) return null
    return (() => {
      const cul = s.cultivate
      const pet = {
        petId: s.petId,
        skills: s.skills,
        // 培养合成模式下引擎拒收非零直传数值——互斥段不带 base
        base: cul?.enabled ? [0, 0, 0, 0, 0, 0]
          : (s.base && s.base.length === 6 ? s.base : [0, 0, 0, 0, 0, 0]),
        traitId: s.traitId || 0,
      }
      // 培养段（培养线，可选）：启用才传，只带协议认识的键
      if (cul?.enabled) {
        pet.cultivate = {
          ev: [...cul.ev],
          natureId: cul.natureId,
          inscriptions: [...cul.inscriptions],
          hpCap20: !!cul.hpCap20,
          guild: !!cul.guild,
          annual: !!cul.annual,
          title: cul.titleId || 0,
          resist: {
            enabled: !!cul.resist.enabled,
            damage: [...(cul.resist.damage || [0, 0, 0])],
            // 引擎线协议格式：[[异常id, 概率], ...] ≤3
            control: (cul.resist.control || []).map((p) => [p.anomalyId, p.pct]),
            uncontrol: (cul.resist.uncontrol || []).map((p) => [p.anomalyId, p.pct]),
            completeImmunity: cul.resist.completeImmunity || 0,
          },
        }
      }
      // 词条参数覆盖（可选段，装配期生效；空数组不传 = 全用库值）
      if (s.effectParams && s.effectParams.length) {
        pet.effectParams = s.effectParams
      }
      // 效果禁用（可选段；空 = 全启用）——boss 对局可整队预填 boss_invalid_effects.json
      const de = s.disabledEffects
      if (de && (de.skillEffects?.length || de.soulMarks?.length)) {
        pet.disabledEffects = {
          skillEffects: [...(de.skillEffects || [])],
          soulMarks: [...(de.soulMarks || [])],
        }
      }
      if (s.oracle) pet.oracle = true
      return pet
      })()
  }
  const s1 = sideLineup('side1').map(build).filter(Boolean)
  const s2 = sideLineup('side2').map(build).filter(Boolean)
  if (!s1.length || !s2.length) throw new Error('两边队伍都要至少 1 只精灵')
  const payload = { side1: s1, side2: s2 }
  if (team.bossChallenge) payload.bossChallenge = true
  // 待命背包快照：未被选入出战的待命宠 id（1551 自爆等"赛前读者"计数域）。
  // direct 模式不传——引擎侧待命为空，语义与"直开 6v6"一致。
  if (team.mode === 'bp') {
    const standbyIds = (side) => {
      const chosen = new Set(sideLineup(side).map((s) => s.petId))
      return standbySlots[side].filter((s) => s.petId > 0 && !chosen.has(s.petId))
          .map((s) => s.petId)
    }
    const sb = { side1: standbyIds('side1'), side2: standbyIds('side2') }
    if (sb.side1.length || sb.side2.length) payload.standby = sb
  }
  const meds = { side1: {}, side2: {} }
  for (const side of ['side1', 'side2']) {
    for (const [id, count] of Object.entries(team.medicines[side])) {
      if (count > 0) meds[side][id] = count
    }
  }
  if (Object.keys(meds.side1).length || Object.keys(meds.side2).length) {
    payload.medicines = meds
  }
  // 装备穿戴清单（可选段；空 = 双方裸装，兼容旧协议）
  if (team.equip.side1.length || team.equip.side2.length) {
    payload.equip = {
      side1: [...team.equip.side1],
      side2: [...team.equip.side2],
    }
  }
  return payload
}

/** 时点/异常/票类型对照表（/api/meta，注册表导出附带） */
export const meta = reactive({
  states: [], anomalies: [], medicines: [],
  // 规则票固定显示表（registry dump 六表，FullStatePanel 用）
  ruleCategories: [], immunityTypes: [], sealKinds: [],
  chanceTags: [], chanceSources: [], effectScopes: [],
  // 引擎指纹（2026-10-01 HELLO 握手；null = 网关未起/老服务端）
  engineFingerprint: null,
})

/** 引擎版本报警（看板式横幅，App.vue 顶部）：引擎指纹与页面加载时不一致 = 显示面
 * 可能落后于引擎（新字段/新枚举漏显）。判定源两处，见 handleEvent 'hello'。 */
export const engineVersionAlert = reactive({ active: false, detail: '' })

const ENGINE_FP_KEY = 'sbs_engine_fingerprint'

export async function loadMeta() {
  try {
    Object.assign(meta, await get('/meta'))
    if (meta.engineFingerprint) {
      // 与上次访问比对：引擎跨会话更新了（本页 JS 若是旧缓存也一并被点名）。
      const prev = localStorage.getItem(ENGINE_FP_KEY)
      if (prev && prev !== JSON.stringify(meta.engineFingerprint)) {
        engineVersionAlert.active = true
        engineVersionAlert.detail =
          `引擎已更新（上次 ${prev} → 本次 ${JSON.stringify(meta.engineFingerprint)}）——` +
          '为避免新字段漏显，请硬刷新页面（Cmd/Ctrl+Shift+R）。'
      }
      localStorage.setItem(ENGINE_FP_KEY, JSON.stringify(meta.engineFingerprint))
    }
  } catch { /* 网关未起 */ }
}
export const anomalyName = (id) => meta.anomalies.find((a) => a.id === id)?.name || `异常#${id}`
export const stateName = (id) => meta.states.find((s) => s.id === id)?.name || `时点${id}`

/** 培养元数据（性格表/上限常量，培养编辑器与预览共用；懒加载一次） */
export const cultivateMeta = reactive({ loaded: false, natures: [], evPerStatCap: 255, evTotalCap: 510, guildBoost: [], hpCapBonus: 20 })
export async function loadCultivateMeta(force = false) {
  if (cultivateMeta.loaded && !force) return cultivateMeta
  try {
    const data = await get('/cultivation/meta')
    Object.assign(cultivateMeta, data, { loaded: true })
  } catch { /* 网关未起 */ }
  return cultivateMeta
}
export const natureName = (id) =>
  cultivateMeta.natures.find((n) => n.id === id)?.name || (id < 0 ? '无（中性）' : `性格#${id}`)

export const ABILITY_SLOTS = ['攻', '特攻', '防', '特防', '速', '命中']
export const MEDICINE_NAMES = {
  0: '体力药剂', 1: 'PP药剂', 2: '解除能力下降', 3: '解除异常',
  4: '体力+PP', 5: 'PP+解降', 6: '体力+解异常',
}

export const battle = reactive({
  phase: 'idle',            // idle | running | over
  snapshot: null,
  over: null,
  tapeLines: [],            // 结构化行对象（sample/mutate），TapePanel.vue 渲染
  rawTape: [],              // 原始 TAPE 采样（未渲染的 JSON 对象）——反馈/对拍的原料，导出用
  lineup: null,             // 本局阵容 payload 快照（导出时还原局面用）
  input: null,              // 服务端正在等谁：{player, legal, reason, round, ...}
  legals: { 0: null, 1: null },   // 双方各自的合法动作集（LEGAL_ACTIONS 拉取）
  pending: { 0: false, 1: false }, // 本地"已提交待结算"标记
  auto: { 0: false, 1: true },     // 默认：对方自动
  stepMode: false,
  breakpoints: [],
  fullstate: '',
  fullstateData: null,      // 全量状态解析对象（FullStatePanel 表格渲染用）
  wsError: '',
  socket: null,
  // 对战动画（2026-10-04）：fx[side] = 漂浮元素队列（{id,type,text}），fxEnabled 总开关
  fx: { 0: [], 1: [] },
  fxEnabled: true,
})

// LEGAL_ACTIONS 响应不带 player 字段，靠请求顺序对齐（WS 保序）
let legalQueue = []

export function requestLegals(players) {
  if (!battle.socket || battle.phase !== 'running') return
  for (const p of players) {
    legalQueue.push(p)
    battle.socket.legal(p)
  }
}

export function startBattle(lineup) {
  stopBattle()
  battle.phase = 'running'
  battle.over = null
  battle.snapshot = null
  battle.input = null
  battle.legals = { 0: null, 1: null }
  battle.pending = { 0: false, 1: false }
  battle.tapeLines = []
  battle.rawTape = []
  battle.fx = { 0: [], 1: [] }
  resetTapeContext()   // hp/levels 增量基准跨局作废（否则新局首行带旧局的 ▼/▲）
  battle.lineup = lineup || null
  battle.breakpoints = []   // 断点镜像随局重置——新 sim 会话里旧断点已不存在
  battle.fullstate = ''
  battle.fullstateData = null
  battle.wsError = ''
  const sock = new BattleSocket(handleEvent, () => {
    if (battle.phase === 'running') battle.wsError = '与网关的连接断开'
  })
  battle.socket = sock
  sock.connect()
  sock.init(lineup)
}

export function stopBattle() {
  if (battle.socket) {
    battle.socket.close()
    battle.socket = null
  }
  battle.phase = 'idle'
  battle.input = null
  legalQueue = []
}

const EVENT_LABELS = {
  EVENT_HIT: '命中',
  EVENT_TAKE_DAMAGE: '受伤',
  EVENT_TAKE_PINK_DAMAGE: '粉伤',
  EVENT_DEATH: '死亡',
  EVENT_CONTROLLED: '控制',
  EVENT_ANOMALY_APPLIED: '施加异常',
  EVENT_ANOMALY_EXPIRED: '异常消退',
  EVENT_ANOMALY_RESISTED: '异常抵抗',
  EVENT_ANOMALY_IMMUNED: '异常免疫',
  EVENT_SHIELD_BROKEN: '破盾',
  EVENT_ENTER_STAGE: '登场',
  EVENT_SWAP: '换宠',
  EVENT_OPPONENT_DEFEATED: '击败对手',
  EVENT_SKILL_INVALID: '技能无效',
  EVENT_ATTACK_BLOCKED: '攻击被挡',
  EVENT_SKILL_ARMOR_RESOLVED: '盔结算',
  EVENT_HP_TO_ZERO: '体力归零',
  EVENT_VANISH: '消逝',
  EVENT_STAT_CHANGED: '能力变化',
  EVENT_BREAK: '打断',
  // 2026-10-04 补标签（此前引擎没发名字，线上落 EVENT_UNKNOWN）
  EVENT_SKILL_USED: '使用技能',
  EVENT_HP_EXHAUSTED: '体力耗尽',
  EVENT_PP_REDUCED: 'PP被降',
  EVENT_BEFORE_SKILL_HIT: '命中前锚点',
  EVENT_HEAL_RESTORED: '恢复体力',
  EVENT_STAT_REMOVED: '强化被消除',
  EVENT_STAT_BOOSTS_LOST: '提升丧失',
}

export function eventLabel(t) { return EVENT_LABELS[t] || t }

/** 伤害分型（引擎 DamageKind 枚举序）→ 显示分类。
 *  红伤 = 攻击伤害；粉伤 = 固定/百分比族（三档共用粉色）；真伤 = 无视护盾护罩；
 *  属性伤 = 属性技能直伤（走红伤落血但护盾不响应，单列颜色）。 */
export const DAMAGE_KINDS = {
  0: { cls: 'red',  label: '红伤' },
  1: { cls: 'pink', label: '粉伤' },
  2: { cls: 'pink', label: '粉伤' },
  3: { cls: 'pink', label: '粉伤' },
  4: { cls: 'true', label: '真伤' },
  5: { cls: 'attr', label: '属性伤' },
}

// ── 事件带（结构化行，2026-10-04 可读性重做）──
// tapeLines 不再是 HTML 字符串，改成行对象由 TapePanel.vue 渲染：
//   { kind:'sample', ... } FSM 时点采样   { kind:'mutate', ok, text } 手术回执
// rawTape 照旧存服务端原始采样（导出/对拍用），本节所有加工都是只读派生。
let prevSample = null        // 上一采样（跨批次连续，算 hp/levels 增量用）
let tapeKeySeq = 0           // 行渲染 key（行可能没有 seq，如手术回执）

/** 采样缺宠名（老引擎/开局 on_stage 未定）→ 退回快照的场上宠名 */
function stageNameFallback() {
  const snap = battle.snapshot
  return [snap?.player0?.pet?.name || '', snap?.player1?.pet?.name || '']
}

/** 与上一采样逐槽比对能力等级，返回 [{side, slot, d}]（只列变化槽） */
function levelDeltas(levels) {
  const out = []
  if (!prevSample || !levels) return out
  for (let side = 0; side < 2; ++side) {
    for (let i = 0; i < 6; ++i) {
      const d = levels[side][i] - prevSample.levels[side][i]
      if (d !== 0) out.push({ side, slot: i, d })
    }
  }
  return out
}

export function pushTape(data) {
  // 原始采样先留存（渲染结构是给看的，对拍/反馈要的是这份数据）
  if (data.samples?.length) {
    battle.rawTape.push(...data.samples)
    if (battle.rawTape.length > 8000) {
      battle.rawTape.splice(0, battle.rawTape.length - 8000)
    }
  }
  const fbNames = stageNameFallback()
  // 批次前血量：动画兜底数字的基准（事件没解释的血量变动用它算）
  const hpBeforeBatch = prevSample ? [...prevSample.hp] : null
  const stageIdsBefore = prevSample ? prevSample.petIds : undefined
  const stageSlotsBefore = prevSample ? prevSample.petSlots : undefined
  // 场上精灵身份 = id+槽位（镜像对局 6 只同 id 宠，光 id 判等区分不出换宠）；
  // 老引擎两者都不发（undefined 判等）按同宠处理。
  const sameStage = (a, b, i) => a?.[i] === b?.[i]
  for (const s of data.samples || []) {
    const names = [0, 1].map((i) => s.petNames?.[i] || fbNames[i] || `p${i}`)
    // 换宠后的方血量增量失去可比性（▲85 全血上场不是回血）——该方 hpPrev 置 null
    const hpPrev = prevSample
      ? [0, 1].map((i) => (sameStage(s.petIds, prevSample.petIds, i) &&
                           sameStage(s.petSlots, prevSample.petSlots, i))
            ? prevSample.hp[i] : null)
      : null
    // resolvedDamage 是管线驻留值（护盾/护罩抵消后），不刷新就一直带在采样里——
    // 只在**发生变化**的采样上显示，否则一个伤害数字会糊满整个行动的所有时点行
    const resolvedPrev = prevSample ? prevSample.resolved_damage : undefined
    const lvDelta = levelDeltas(s.levels)
    const events = (s.events || []).map((e) => ({ ...e, label: eventLabel(e.type) }))
    const hpChanged = !!hpPrev && (s.hp[0] !== hpPrev[0] || s.hp[1] !== hpPrev[1])
    battle.tapeLines.push({
      kind: 'sample', key: 's' + (tapeKeySeq++),
      seq: s.seq, round: s.round, stateName: s.stateName, actor: s.actor,
      hp: s.hp, maxHp: s.maxHp, hpPrev, names,
      pending: s.pendingDamage || 0, resolved: s.resolvedDamage || 0,
      resolvedPrev,
      lvDelta, events,
      // 精简模式的"关键行"判据：有事件/血量动了/等级动了 才值得看
      // （resolved 不参与——它是驻留值，见上）
      interesting: events.length > 0 || hpChanged || lvDelta.length > 0,
    })
    prevSample = s
  }
  if (battle.tapeLines.length > 4000) {
    battle.tapeLines.splice(0, battle.tapeLines.length - 4000)
  }
  applyFx(data.samples || [], hpBeforeBatch, stageIdsBefore, stageSlotsBefore)
}

export function resetTapeContext() { prevSample = null }

// ── 对战动画（fx）：把一批采样聚合红/粉/真伤数字、命中抖动、治疗、能力变化等，
//    挂到 battle.fx[side] 队列，BattlePanel 的覆盖层消费。逐时点采样在"逐步模式"
//    下天然一批一个时点，连点推进就是逐时点动画；自动模式一批 = 一次行动聚合，
//    数字取合计，避免几十段连击把动画拖成幻灯片。──
let fxSeq = 0
function pushFx(side, type, text, ttl = 1500) {
  const item = { id: 'fx' + (fxSeq++), type, text }
  battle.fx[side].push(item)
  if (battle.fx[side].length > 8) battle.fx[side].splice(0, battle.fx[side].length - 8)
  setTimeout(() => {
    const list = battle.fx[side]
    const i = list.indexOf(item)
    if (i >= 0) list.splice(i, 1)
  }, ttl)
}

function applyFx(samples, hpBefore, stageIdsBefore, stageSlotsBefore) {
  if (!battle.fxEnabled || !samples.length) return
  const agg = {
    dmg: [{}, {}],            // side → { red: n, pink: n, true: n, attr: n }
    heal: [0, 0],
    hit: [false, false],      // side 被命中（抖动）
    shield: [false, false],
    death: [false, false],
    stats: [[], []],          // side → [{slot, d}]
    skill: ['', ''],          // side → 该方最后使用的技能名（命中动画标注）
  }
  let last = null
  for (const s of samples) {
    last = s
    for (const e of s.events || []) {
      const t = e.target
      if (e.type === 'EVENT_TAKE_DAMAGE') {
        const k = (DAMAGE_KINDS[e.damageKind ?? 0] || DAMAGE_KINDS[0]).cls
        agg.dmg[t][k] = (agg.dmg[t][k] || 0) + (e.amount || 0)
      } else if (e.type === 'EVENT_TAKE_PINK_DAMAGE') {
        agg.dmg[t].pink = (agg.dmg[t].pink || 0) + (e.amount || 0)
      } else if (e.type === 'EVENT_HEAL_RESTORED') {
        agg.heal[t] += e.amount || 0
      } else if (e.type === 'EVENT_HIT') {
        agg.hit[t] = true
      } else if (e.type === 'EVENT_SHIELD_BROKEN') {
        agg.shield[t] = true
      } else if (e.type === 'EVENT_DEATH' || e.type === 'EVENT_VANISH') {
        agg.death[t] = true
      } else if (e.type === 'EVENT_STAT_CHANGED' && e.levelDelta) {
        agg.stats[t].push({ slot: e.statIndex ?? 0, d: e.levelDelta })
      } else if (e.type === 'EVENT_SKILL_USED' && e.skillName) {
        agg.skill[e.actor] = e.skillName
      }
    }
  }
  const hpAfter = last ? [...last.hp] : null
  for (const side of [0, 1]) {
    const d = agg.dmg[side]
    // 命中抖动 + 技能名标注（谁打的我）
    if (agg.hit[side]) {
      pushFx(side, 'hit', agg.skill[1 - side] || '', 900)
    }
    if (d.red) pushFx(side, 'dmg-red', '-' + d.red)
    if (d.true) pushFx(side, 'dmg-true', '真 -' + d.true)
    if (d.attr) pushFx(side, 'dmg-attr', '属 -' + d.attr)
    if (d.pink) pushFx(side, 'dmg-pink', '-' + d.pink)
    if (agg.heal[side]) pushFx(side, 'heal', '+' + agg.heal[side])
    // 血量动了但事件没解释（体力修正通道：自爆 HP_CONSUME、斩杀印记等）→ 兜底通用数字。
    // 批内换过宠的方不算（全血上场会被误判成"回血 +N"）。身份 = id+槽位。
    if (hpBefore && hpAfter && !d.red && !d.pink && !d.true && !d.attr && !agg.heal[side]
        && last.petIds?.[side] === stageIdsBefore?.[side]
        && last.petSlots?.[side] === stageSlotsBefore?.[side]) {
      const delta = hpAfter[side] - hpBefore[side]
      if (delta < 0) pushFx(side, 'dmg-red', '-' + (-delta))
      else if (delta > 0) pushFx(side, 'heal', '+' + delta)
    }
    // 能力变化气泡（同槽取净值，最多展示 3 个）
    const netBySlot = {}
    for (const { slot, d: dv } of agg.stats[side]) netBySlot[slot] = (netBySlot[slot] || 0) + dv
    Object.entries(netBySlot).slice(0, 3).forEach(([slot, dv]) => {
      if (dv) pushFx(side, dv > 0 ? 'stat-up' : 'stat-down',
        `${ABILITY_SLOTS[slot]}${dv > 0 ? '+' + dv : dv}`, 1800)
    })
    if (agg.shield[side]) pushFx(side, 'shield', '🛡 破盾', 1600)
    if (agg.death[side]) pushFx(side, 'death', '倒下', 2400)
  }
}

/** 导出本局原始数据（阵容 + 全部 TAPE 采样 + 结果）——差异报告/对拍的附件格式 */
export function exportBattleTape() {
  const dump = {
    exportedAt: new Date().toISOString(),
    lineup: battle.lineup,
    over: battle.over,
    sampleCount: battle.rawTape.length,
    samples: battle.rawTape,
  }
  const blob = new Blob([JSON.stringify(dump, null, 2)], { type: 'application/json' })
  const a = document.createElement('a')
  a.href = URL.createObjectURL(blob)
  a.download = `seer-battle-${new Date().toISOString().replace(/[:.]/g, '-')}.json`
  a.click()
  URL.revokeObjectURL(a.href)
}

function handleEvent(type, data) {
  if (type === 'hello') {
    // 版本握手（WS 开局时网关推的第一条）：与启动时 meta 的指纹比——不一致说明
    // 引擎在本页停留期间被单独更新，显示面已是旧口径，横幅点名要求刷新。
    if (data && meta.engineFingerprint) {
      const now = JSON.stringify(data)
      const boot = JSON.stringify(meta.engineFingerprint)
      if (now !== boot) {
        engineVersionAlert.active = true
        engineVersionAlert.detail =
          `引擎在本页停留期间被更新（${boot} → ${now}）——显示面已落后，请刷新页面重新开局。`
      }
    }
    return
  }
  if (type === 'room_info') {
    battle.wsError = ''
  } else if (type === 'tape') {
    pushTape(data)
    battle.pending = { 0: false, 1: false }
    if (battle.phase === 'running') requestLegals([0, 1])
  } else if (type === 'input_required') {
    battle.snapshot = data.snapshot
    battle.input = data
    battle.legals[data.player] = data.legal
    battle.pending = { 0: false, 1: false }
    const other = data.player === 0 ? 1 : 0
    requestLegals([other])
    maybeAutoAnswer(data)
  } else if (type === 'mutate_result') {
    // 调试手术回执：逐条操作回执进事件带（🔧 前缀），成功后服务端已推新快照
    if (data.ok) {
      const notes = data.notes || []
      battle.tapeLines.push({
        kind: 'mutate', key: 'm' + (tapeKeySeq++), ok: true,
        text: notes.length ? notes.join('；') : '（空操作集）',
      })
      requestLegals([0, 1])   // PP/异常/等级被改 → 合法动作集可能变化，刷新
    } else {
      battle.tapeLines.push({
        kind: 'mutate', key: 'm' + (tapeKeySeq++), ok: false,
        text: `手术被拒：${data.error || '未知原因'}`,
      })
    }
  } else if (type === 'legal') {
    const p = legalQueue.length ? legalQueue.shift() : 0
    battle.legals[p] = data
  } else if (type === 'state') {
    battle.snapshot = data
  } else if (type === 'fullstate') {
    battle.fullstateData = data
    battle.fullstate = JSON.stringify(data, null, 2)
  } else if (type === 'battle_over') {
    battle.snapshot = data.snapshot
    battle.over = data
    battle.phase = 'over'
    battle.input = null
    battle.pending = { 0: false, 1: false }
    recordBattleOver()   // 完成一局：反馈提醒计数（feedbackPrompt.js，纯客户端）
  } else if (type === 'error') {
    battle.wsError = data.text || data.hex || '未知错误'
  } else if (type === 'sim_closed') {
    if (battle.phase === 'running') battle.wsError = 'sim_server 连接断开'
  }
}

/** 切换某方自动应答；打开时立即补答当前已挂起的输入（否则本回合会卡住等人）。 */
export function setAuto(side, val) {
  battle.auto[side] = val
  if (val && battle.input && battle.input.player === side && battle.phase === 'running') {
    answerWithFirstLegal(side, battle.input.legal)
  }
}

/** 自动应答：技能取第一个可用；死后换宠取第一个可用座位。 */
export function maybeAutoAnswer(input) {
  const player = input.player
  if (!battle.auto[player]) return
  answerWithFirstLegal(player, input.legal)
}

export function answerWithFirstLegal(player, legal) {
  if (legal.mustChoosePet) {
    const pet = (legal.pets || []).find((p) => p.usable && !p.onStage)
    if (pet) submitAction(player, 0, pet.slot)
    return
  }
  const skill = (legal.skills || []).find((s) => s.usable)
  if (skill) {
    submitAction(player, 1, skill.index)
  } else {
    // 没有任何可用技能（PP 被清空等）→ 兜底空操作，模拟超时，避免自动应答静默卡死
    submitAction(player, 3, 0)
  }
}

/** 空操作（超时/什么都不做）：回合类与异常后续照常。 */
export function submitNone(robot) {
  submitAction(robot, 3, 0)
}

/**
 * 自由选操作方提交：robot 可以是任意一方（服务端会把没轮到的动作排队，
 * 支持先给对手提交换宠、再给自己提交的触发顺序实验）。
 */
export function submitAction(robot, actionType, index) {
  if (!battle.socket || battle.phase !== 'running') return
  battle.socket.action(robot, actionType, index)
  battle.pending[robot] = true
  if (battle.input && battle.input.player === robot) battle.input = null
}

/** 调试手术：白名单操作集，泊车时由服务端执行（见 battle_mutate.h）。 */
export function sendMutate(ops) {
  if (!battle.socket || battle.phase !== 'running') return
  battle.socket.mutate(ops)
}

export function setStepMode(on) {
  battle.stepMode = on
  if (battle.socket) on ? battle.socket.step() : battle.socket.continue()
}

/** 断点同步：toggle 语义，只发与当前镜像的差异 */
export function applyBreakpoints(targetIds) {
  if (!battle.socket) return
  const cur = new Set(battle.breakpoints)
  for (const id of targetIds) {
    if (!cur.has(id)) battle.socket.breakpoint(id)
  }
  for (const id of [...battle.breakpoints]) {
    if (!targetIds.includes(id)) battle.socket.breakpoint(id)
  }
  battle.breakpoints = [...targetIds]
}
