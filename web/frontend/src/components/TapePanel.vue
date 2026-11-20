<script setup>
// 事件带渲染（2026-10-04 可读性重做）：
//   · 回合分组头 + 精简模式（默认）：无事件/无血变/无伤害/无等级变化的"过程时点"
//     折叠成一行计数，点击展开——一个技能几十个 FSM 时点不再淹没关键行。
//   · 宠名代替 p0/p1（引擎采样自带，老引擎退回快照名）；血量按比例着色 + 增量箭头。
//   · 伤害按分型着色（红/粉/真/属性），事件按语义着色，替代原来一锅粥的 [事件 p0→p1]。
import { ref, computed, watch, nextTick } from 'vue'
import { battle, exportBattleTape, anomalyName, ABILITY_SLOTS, DAMAGE_KINDS } from '../store'

const mode = ref('brief')          // brief=精简（默认）| full=全部时点
const expanded = ref(new Set())    // 手动展开的过程时点段（存段首行 key）
const scrollBox = ref(null)

function toggleGap(key) {
  const s = new Set(expanded.value)
  if (s.has(key)) s.delete(key)
  else s.add(key)
  expanded.value = s
}

// 渲染序列：sample 行之间插回合头；精简模式把连续非关键行折叠为 gap 行。
// 展开状态记在**段首行**上，命中即整段还原（不是逐行拆，拆了等于没展开）。
const viewRows = computed(() => {
  const out = []
  let lastRound = null
  let group = null                 // 连续非关键行的缓冲
  const flush = () => {
    if (!group) return
    if (expanded.value.has(group[0].key)) out.push(...group)
    else out.push({ kind: 'gap', key: 'g' + group[0].key, atKey: group[0].key, count: group.length })
    group = null
  }
  for (const row of battle.tapeLines) {
    if (row.kind !== 'sample') {
      flush()
      out.push(row)
      continue
    }
    if (row.round !== lastRound) {
      flush()
      lastRound = row.round
      out.push({ kind: 'round', key: 'r' + row.key, round: row.round })
    }
    if (mode.value === 'full' || row.interesting) {
      flush()
      out.push(row)
    } else {
      (group ??= []).push(row)
    }
  }
  flush()
  return out
})

watch(() => battle.tapeLines.length, async () => {
  await nextTick()
  if (scrollBox.value) scrollBox.value.scrollTop = scrollBox.value.scrollHeight
})

const hpRatio = (row, side) => (row.maxHp[side] > 0 ? row.hp[side] / row.maxHp[side] : 0)
const hpColor = (v) => (v > 0.5 ? '#67c23a' : v > 0.25 ? '#e6a23c' : '#f56c6c')
// hpPrev 按侧可空：换宠后的方不比增量（新生全血上场不是回血）
const hpDelta = (row, side) => {
  const prev = row.hpPrev?.[side]
  return prev == null ? 0 : row.hp[side] - prev
}
const sideTag = (s) => (s === 0 ? '我方' : '对方')

// 一次伤害事件 → 分型 chip（TAKE_DAMAGE 带分型；PINK 单独事件；老引擎缺分型按红伤）
function damageChips(row) {
  const chips = []
  let hasTake = false
  for (const e of row.events) {
    if (e.type === 'EVENT_TAKE_DAMAGE') {
      hasTake = true
      const k = DAMAGE_KINDS[e.damageKind ?? 0] || DAMAGE_KINDS[0]
      chips.push({ cls: k.cls, text: `${k.label} -${e.amount}→${sideTag(e.target)}`, title: e.label })
    } else if (e.type === 'EVENT_TAKE_PINK_DAMAGE') {
      hasTake = true
      chips.push({ cls: 'pink', text: `粉伤 -${e.amount}→${sideTag(e.target)}`, title: e.label })
    }
  }
  // 管线视图兜底：本采样伤害值**刚变化**且没带 TAKE 事件（老引擎）→ 显示原口径。
  // resolved 是驻留值，不判变化会把同一个数字糊满整个行动的所有时点行。
  if (!hasTake && row.resolved > 0 && row.resolved !== row.resolvedPrev) {
    const absorbed = row.pending - row.resolved
    chips.push({
      cls: 'red',
      text: absorbed > 0 ? `伤害=${row.resolved}（盾吸${absorbed}）` : `伤害=${row.resolved}`,
    })
  }
  return chips
}

const LEVEL_EVENT_TYPES = new Set(['EVENT_TAKE_DAMAGE', 'EVENT_TAKE_PINK_DAMAGE', 'EVENT_STAT_CHANGED'])

// 其余事件 → 语义 chip（伤害/等级族在上面单独渲染，这里跳过）
function eventChips(row) {
  const chips = []
  for (const e of row.events) {
    if (LEVEL_EVENT_TYPES.has(e.type)) continue
    const who = (side) => (side >= 0 ? `→${sideTag(side)}` : '')
    switch (e.type) {
      case 'EVENT_SKILL_USED':
        chips.push({ cls: 'skill', text: `${sideTag(e.actor)}·${e.skillName || '技能'}${e.usedAttribute ? '（属性）' : ''}` })
        break
      case 'EVENT_HIT':
        chips.push({ cls: 'hit', text: `命中${who(e.target)}` })
        break
      case 'EVENT_HEAL_RESTORED':
        chips.push({ cls: 'heal', text: `恢复 +${e.amount}${who(e.target)}` })
        break
      case 'EVENT_DEATH':
        chips.push({ cls: 'bad', text: `倒下${who(e.target)}` })
        break
      case 'EVENT_VANISH':
        chips.push({ cls: 'bad', text: `消逝${who(e.target)}` })
        break
      case 'EVENT_HP_TO_ZERO':
      case 'EVENT_HP_EXHAUSTED':
        chips.push({ cls: 'bad', text: `${e.label}${who(e.target)}` })
        break
      case 'EVENT_CONTROLLED':
      case 'EVENT_ANOMALY_APPLIED':
        chips.push({ cls: 'bad', text: `${anomalyName(e.amount)}${who(e.target)}` })
        break
      case 'EVENT_ANOMALY_EXPIRED':
        chips.push({ cls: 'info', text: `${anomalyName(e.amount)}消退${who(e.target)}` })
        break
      case 'EVENT_ANOMALY_RESISTED':
        chips.push({ cls: 'info', text: `抵抗 ${anomalyName(e.amount)}${who(e.target)}` })
        break
      case 'EVENT_ANOMALY_IMMUNED':
        chips.push({ cls: 'info', text: `免疫 ${anomalyName(e.amount)}${who(e.target)}` })
        break
      case 'EVENT_SHIELD_BROKEN':
        chips.push({ cls: 'warn', text: `破盾${who(e.target)}` })
        break
      case 'EVENT_ENTER_STAGE':
        chips.push({ cls: 'good', text: `登场${who(e.target)}` })
        break
      case 'EVENT_SWAP':
        chips.push({ cls: 'info', text: `换宠${who(e.target)}` })
        break
      case 'EVENT_OPPONENT_DEFEATED':
        chips.push({ cls: 'good', text: `击败${who(e.target)}` })
        break
      case 'EVENT_SKILL_INVALID':
        chips.push({ cls: 'warn', text: `技能无效${who(e.actor)}` })
        break
      case 'EVENT_ATTACK_BLOCKED':
        chips.push({ cls: 'warn', text: `攻击被挡${who(e.target)}` })
        break
      case 'EVENT_SKILL_ARMOR_RESOLVED':
        chips.push({ cls: 'warn', text: e.blocked ? `盔生效${who(e.target)}` : `盔被穿${who(e.target)}` })
        break
      case 'EVENT_STAT_REMOVED':
      case 'EVENT_STAT_BOOSTS_LOST':
        chips.push({ cls: 'warn', text: `${e.label}×${e.amount}${who(e.target)}` })
        break
      case 'EVENT_PP_REDUCED':
        chips.push({ cls: 'warn', text: `PP-${e.amount}${who(e.target)}` })
        break
      case 'EVENT_BREAK':
        chips.push({ cls: 'warn', text: `断回合${who(e.target)}` })
        break
      default:
        // EVENT_BEFORE_SKILL_HIT 等内部锚点不上屏（纯噪声）；未知类型保底显示
        if (e.type !== 'EVENT_BEFORE_SKILL_HIT') {
          chips.push({ cls: 'info', text: e.label })
        }
    }
  }
  return chips
}

// 组装一行的全部 chip：伤害数字 + 等级变化 + 语义事件
function chipsOf(row) {
  const lv = row.lvDelta.map(({ side, slot, d }) => ({
    cls: d > 0 ? 'up' : 'down',
    text: `${sideTag(side)}${ABILITY_SLOTS[slot]}${d > 0 ? '+' + d : d}`,
  }))
  return [...damageChips(row), ...lv, ...eventChips(row)]
}
</script>

<template>
  <div class="tape-box mono">
    <div class="tape-toolbar">
      <el-radio-group v-model="mode" size="small">
        <el-radio-button value="brief">精简</el-radio-button>
        <el-radio-button value="full">全部时点</el-radio-button>
      </el-radio-group>
      <el-tooltip content="对战面板上的伤害数字/抖动等动画开关" placement="top">
        <span style="display:inline-flex; align-items:center; gap:4px;">
          动画 <el-switch v-model="battle.fxEnabled" size="small" />
        </span>
      </el-tooltip>
      <span style="margin-left:auto;">
        <el-button size="small" text type="primary" :disabled="!battle.rawTape.length"
                   @click="exportBattleTape">导出本局 JSON</el-button>
      </span>
    </div>
    <div ref="scrollBox" class="tape-scroll">
      <template v-for="row in viewRows" :key="row.key">
        <div v-if="row.kind === 'round'" class="tape-round">── 回合 {{ row.round }} ──</div>
        <div v-else-if="row.kind === 'gap'" class="tape-gap" @click="toggleGap(row.atKey)">
          ⋯ {{ row.count }} 个过程时点（点击展开）
        </div>
        <div v-else-if="row.kind === 'mutate'" class="tape-line tape-mutate"
             :class="{ 'tape-mutate-err': !row.ok }">🔧 {{ row.text }}</div>
        <div v-else class="tape-line">
          <span class="seq">#{{ row.seq }}</span>
          <span class="st">{{ row.stateName }}</span>
          <span v-for="s in [0, 1]" :key="s" class="pet" :class="s === 0 ? 'pet-0' : 'pet-1'">
            {{ sideTag(s) }}·{{ row.names[s] }}
            <b class="hp" :style="{ color: hpColor(hpRatio(row, s)) }">{{ row.hp[s] }}</b><span class="hp-max">/{{ row.maxHp[s] }}</span><span
              v-if="hpDelta(row, s)" class="hp-delta" :class="hpDelta(row, s) < 0 ? 'down' : 'up'">
              {{ hpDelta(row, s) < 0 ? '▼' : '▲' }}{{ Math.abs(hpDelta(row, s)) }}</span>
          </span>
          <span v-for="(c, i) in chipsOf(row)" :key="i" class="chip" :class="'chip-' + c.cls"
                :title="c.title">{{ c.text }}</span>
        </div>
      </template>
      <div v-if="!battle.tapeLines.length" class="tape-empty">
        事件带：开局后这里按 FSM 时点逐行显示判定过程（默认精简模式，只看关键行）
      </div>
    </div>
  </div>
</template>

<style scoped>
.tape-toolbar {
  display: flex; align-items: center; gap: 12px;
  padding: 2px 6px 6px;
}
.tape-scroll { overflow-y: auto; height: 430px; }
.tape-round {
  color: #e8c46a; margin: 8px 0 2px; font-weight: 600;
  border-bottom: 1px dashed #3a4356; padding-bottom: 2px;
}
.tape-gap {
  color: #5a6b85; cursor: pointer; padding: 1px 0 1px 14px; font-size: 11px;
}
.tape-gap:hover { color: #8fd0ff; }
.tape-mutate { color: #95de64; }
.tape-mutate-err { color: #ff7875; }
.tape-empty { color: #556; }
.tape-line { white-space: pre-wrap; word-break: break-all; }
.seq { color: #6b7f9e; margin-right: 6px; }
.st { color: #7d8ba1; margin-right: 8px; }
.pet { margin-right: 10px; }
.pet-0 { color: #79b8ff; }
.pet-1 { color: #ff9d9d; }
.pet .hp { font-weight: 700; }
.hp-max { color: #667; }
.hp-delta { font-size: 11px; }
.hp-delta.down { color: #ff7875; }
.hp-delta.up { color: #95de64; }
.chip {
  display: inline-block; border-radius: 3px; padding: 0 5px; margin: 0 4px 0 0;
  font-size: 11px; line-height: 18px; vertical-align: 1px;
  background: #1d2735; border: 1px solid #2c3a4f;
}
.chip-red  { color: #ff7875; border-color: #5c2f2f; }
.chip-pink { color: #ff85c0; border-color: #5c2f4a; }
.chip-true { color: #d3baff; border-color: #45356b; }
.chip-attr { color: #ffc069; border-color: #5c4a2f; }
.chip-heal { color: #95de64; border-color: #2f5c2f; }
.chip-up   { color: #95de64; }
.chip-down { color: #e8b26a; }
.chip-hit  { color: #8fd0ff; }
.chip-skill{ color: #d8b4fe; }
.chip-warn { color: #ffc53d; }
.chip-bad  { color: #ff7875; }
.chip-good { color: #95de64; }
.chip-info { color: #8fb8d8; }
</style>
