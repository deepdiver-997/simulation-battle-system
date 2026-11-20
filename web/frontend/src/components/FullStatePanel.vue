<script setup>
// 全量状态表格化视图（开工文档 §2.2：分组折叠，不一股脑铺开）。
// 数据面 = C_DEBUG_FULLSTATE（id+数值），文案在本组件解析：
//   效果桶/票 → effect_info 模板（技能空间）或 effect_icon tips（魂印空间）；
//   票类别/细类 → registry dump 固定表（meta.ruleCategories 等）；
//   印记 marks → 引擎侧已带中文名，直接展示 count。
import { computed } from 'vue'
import { battle, meta, ABILITY_SLOTS } from '../store'
import { skillEffectText, soulEffectText, moveName } from '../effectText'

const fs = computed(() => battle.fullstateData)

const SIDES = [
  { key: 'player0', label: '🟦 我方（p0）' },
  { key: 'player1', label: '🟥 对方（p1）' },
]

// 四个时点桶共用一张表结构；soul=true 的桶按魂印空间（sourceId → tips）渲染
const BUCKETS = [
  { key: 'skillEffects', title: '技能效果桶', soul: false, source: 'skill' },
  { key: 'soulMarkEffects', title: '魂印效果桶', soul: true, source: 'soul' },
  { key: 'suitEffects', title: '套装效果桶', soul: false, source: 'suit' },
  { key: 'updaterEffects', title: '更新器桶（魂印每回合刷新）', soul: true, source: 'soul' },
]

const RULE_SUBTYPE_TABLE = { 0: 'immunityTypes', 1: 'sealKinds', 15: 'chanceTags' }

function bucketTotal(bucketJson) {
  return bucketJson
    ? Object.values(bucketJson).reduce((n, g) => n + (g.count || 0), 0) : 0
}

function bucketGroups(bucketJson) {
  if (!bucketJson) return []
  const groups = Object.values(bucketJson)
  groups.sort((a, b) => (a.state - b.state) || (a.player - b.player))
  return groups
}

const groupLabel = (g) =>
  `时点${g.state} ${g.stateName} · ${g.player === 0 ? '我方' : '对方'}（${g.count}）`

/** 条目主文案：soul 桶按 sourceId 查魂印 tips；技能桶按 effectId+args 查模板。 */
function effectMainText(e, bucket) {
  if (bucket.soul) return soulEffectText(e.sourceId)
  return skillEffectText(e.effectId, e.args)
}

function effectFallback(e, bucket) {
  if (bucket.source === 'skill') return `效果#${e.effectId}`
  if (bucket.source === 'suit') return `套装#${e.sourceId}`
  return `魂印#${e.sourceId}`
}

/** 魂印程序节点 effectId = 魂印id×100+节点序（soul_mark_manager 注册约定）。 */
function soulNodeSuffix(e) {
  const node = e.effectId - e.sourceId * 100
  return e.sourceId > 0 && node > 0 && node < 100 ? `节点${node}` : ''
}

function effectSource(e, bucket) {
  if (bucket.source === 'soul') return `魂印#${e.sourceId}`
  if (bucket.source === 'suit') return `套装#${e.sourceId}`
  return moveName(e.sourceId) || `技能#${e.sourceId}`
}

function remainingTag(e, round) {
  if (e.invalidated) return { text: '已作废', type: 'danger' }
  if (e.isExpired) return { text: '已过期', type: 'info' }
  if (e.actionOneShot) return { text: '本动作', type: 'warning' }
  if (e.duration < 0) return { text: '常驻', type: 'success' }
  const remain = Math.max(1, e.duration - (round - e.registeredRound))
  return { text: `剩 ${remain} 回合`, type: 'primary' }
}

const scopeName = (s) => meta.effectScopes?.find((x) => x.id === s)?.name || `scope${s}`

function ticketCategory(t) {
  return meta.ruleCategories?.find((x) => x.id === t.category)?.name || `类别${t.category}`
}

function ticketSubtype(t) {
  const key = RULE_SUBTYPE_TABLE[t.category]
  if (!key) return ''
  return meta[key]?.find((x) => x.id === t.subtype)?.name || `#${t.subtype}`
}

function ticketText(t) {
  return (t.soul ? soulEffectText(t.sourceEffectId) : skillEffectText(t.sourceEffectId))
    || `票源#${t.sourceEffectId}`
}

/** 票面数值行：counts/rounds + 类别专属载荷 + 审计维度。 */
function ticketPayload(t) {
  const parts = []
  if (t.counts > 0) parts.push(`剩${t.counts}次`)
  if (t.rounds > 0) parts.push(`剩${t.rounds}回合`)
  if (t.category === 0) {
    if (t.soul) parts.push('魂免')
    if (t.anomalyMask) parts.push(`异常掩码 ${maskNames(t.anomalyMask)}`)
    if (t.coverage) parts.push(`coverage=${t.coverage}`)
    if (t.conditional) parts.push('条件')
  } else if (t.category === 1) {
    if (t.penetrable) parts.push('可穿透')
    if (t.consumedWhenPierced) parts.push('穿透即耗')
    if (t.conditional) parts.push('条件')
  } else if (t.category === 5 || t.category === 6) {
    parts.push(`穿透等级${t.penLevel}`)
    if (t.ignoreAttackImmunity) parts.push('无视免攻')
    if (t.ignoreDamageLimit) parts.push('无视伤害限制')
  } else if (t.category === 15) {
    parts.push(`阈值${t.thresholdPct}% 低于→${t.belowResult} 高于→${t.aboveResult}`)
  } else if (t.category === 8) {
    if (t.poolMod) parts.push('池改写')
  } else if (t.category === 10) {
    if (t.coverage) parts.push(`coverage=${t.coverage}`)
  }
  if (t.tier) parts.push(`tier${t.tier}`)
  parts.push(`授予#${t.grantId}`)
  return parts.join(' · ')
}

const maskNames = (mask) => (meta.anomalies || [])
  .filter((a) => a.id >= 0 && a.id < 32 && (mask & (1 << a.id)))
  .map((a) => a.name).join('/') || String(mask)

function marksText(pet) {
  return (pet.marks || []).map((m) => (m.count > 0 ? `${m.name}×${m.count}` : m.name)).join('、')
}

function abnormalText(pet) {
  return (pet.abnormalStates || []).map((a) => `${a.name}剩${a.remaining}`).join('、')
}

const medicineName = (id) =>
  meta.medicines?.find((m) => m.itemId === id)?.name || `药剂#${id}`

function medicinesText(side) {
  const list = fs.value?.[side.key]?.medicines || []
  return list.map((m) => `${medicineName(m.id)}×${m.count}`).join('、') || '无'
}

/* 结算视图（workspace）键值行 */
const wsRows = computed(() => {
  const ws = fs.value?.workspace
  if (!ws) return []
  const rows = []
  const dmg = (d) => `p${d.attacker}→p${d.defender} final=${d.final}` +
    (d.isCrit ? ' 暴击' : '') + (d.isTrueDamage ? ' 真伤' : '')
  rows.push(['待结算伤害', dmg(ws.damage.pending)])
  rows.push(['已结算伤害', dmg(ws.damage.resolved)])
  const cred = (c) => !c.valid ? '无效' :
    ['ignoreAttackImmunity:无视免攻', 'ignoreDamageLimit:无视限制', 'forceExecute:强制执行', 'mustHit:必中']
      .filter((kv) => c[kv.split(':')[0]]).map((kv) => kv.split(':')[1])
      .concat(`level=${c.level}`).join('、') || '（无附加）'
  rows.push(['攻击凭证 p0', cred(ws.attackCredentials?.player0 || {})])
  rows.push(['攻击凭证 p1', cred(ws.attackCredentials?.player1 || {})])
  rows.push(['威力视图', `p0=${ws.views?.skillPowerView?.player0?.value}${ws.views?.skillPowerView?.player0?.rewritten ? '（改写）' : ''}` +
    ` / p1=${ws.views?.skillPowerView?.player1?.value}${ws.views?.skillPowerView?.player1?.rewritten ? '（改写）' : ''}`])
  rows.push(['必中暴击', `必暴[${+ws.views?.mustCrit?.[0]}/${+ws.views?.mustCrit?.[1]}] 已暴[${+fs.value?.critHappened?.[0]}/${+fs.value?.critHappened?.[1]}]`])
  rows.push(['执行技能槽', `p0=${ws.executingSkillSlot?.[0]} p1=${ws.executingSkillSlot?.[1]}`])
  rows.push(['先制等级', `p0=${ws.preemptiveLevel?.[0]} p1=${ws.preemptiveLevel?.[1]}`])
  const vl = (a) => a.map((v, i) => v ? `${ABILITY_SLOTS[i]}${v > 0 ? '+' + v : v}` : null)
    .filter(Boolean).join(' ') || '—'
  rows.push(['视图等级 p0', vl(ws.viewLevels?.player0 || [])])
  rows.push(['视图等级 p1', vl(ws.viewLevels?.player1 || [])])
  return rows
})

const summaryText = computed(() => {
  if (!fs.value) return ''
  const parts = [`回合 ${fs.value.round}`, fs.value.stateName]
  if (fs.value.needInput) parts.push(`等待 p${fs.value.inputPlayer} 输入`)
  if (fs.value.needDeathSwitch?.player0) parts.push('p0 需换宠')
  if (fs.value.needDeathSwitch?.player1) parts.push('p1 需换宠')
  return parts.join(' · ')
})
</script>

<template>
  <div v-if="fs" style="display:flex; flex-direction:column; gap:10px;">
    <div class="mono" style="color:#606266;">{{ summaryText }}</div>

    <!-- 双方：场上状态 + 全队表 -->
    <el-row :gutter="10">
      <el-col v-for="side in SIDES" :key="side.key" :span="12">
        <div class="section-title" style="margin-top:0;">{{ side.label }}</div>
        <template v-for="pet in fs[side.key]?.party || []" :key="pet.slot">
          <div v-if="pet.onStage" style="margin-bottom:6px;">
            <div style="display:flex; align-items:center; gap:6px; flex-wrap:wrap;">
              <b>▶ {{ pet.name }}</b>
              <span class="mono">{{ pet.hp }}/{{ pet.maxHp }}</span>
              <el-tag v-if="pet.shield > 0" size="small" type="warning" effect="plain">
                🛡 盾 {{ pet.shield }}
              </el-tag>
              <el-tag v-if="pet.cover > 0" size="small" type="warning" effect="plain">
                🧊 罩 {{ pet.cover }}
              </el-tag>
              <template v-for="(lv, i) in pet.levels || []" :key="i">
                <el-tag v-if="lv !== 0" size="small" :type="lv > 0 ? 'success' : 'info'" effect="plain">
                  {{ ABILITY_SLOTS[i] }}{{ lv > 0 ? '+' + lv : lv }}
                </el-tag>
              </template>
            </div>
            <div style="display:flex; gap:4px; flex-wrap:wrap; margin-top:4px;">
              <el-tag v-for="ab in pet.abnormalStates || []" :key="ab.id" size="small" type="danger" effect="plain">
                {{ ab.name }} · 剩{{ ab.remaining }}回合
              </el-tag>
              <el-tag v-for="(m, mi) in pet.marks || []" :key="'m' + mi" size="small" type="success" effect="plain">
                印记 {{ m.count > 0 ? `${m.name}×${m.count}` : m.name }}
              </el-tag>
              <el-tag v-if="!(pet.abnormalStates || []).length && !(pet.marks || []).length"
                      size="small" type="info" effect="plain">无异常/印记</el-tag>
            </div>
          </div>
        </template>
        <div class="mono" style="color:#909399; margin:2px 0;">
          药剂：{{ medicinesText(side) }}
        </div>
        <el-table :data="fs[side.key]?.party || []" size="small" border
                  :row-class-name="({ row }) => row.onStage ? 'onstage-row' : (row.alive ? '' : 'dead-row')">
          <el-table-column prop="slot" label="槽" width="44" />
          <el-table-column label="精灵" min-width="110">
            <template #default="{ row }">
              {{ row.name }}
              <el-tag v-if="row.onStage" size="small" type="primary" effect="plain">场</el-tag>
              <el-tag v-if="!row.alive" size="small" type="danger" effect="plain">亡</el-tag>
            </template>
          </el-table-column>
          <el-table-column label="体力" width="90">
            <template #default="{ row }">
              <span class="mono">{{ row.hp }}/{{ row.maxHp }}</span>
            </template>
          </el-table-column>
          <el-table-column label="魂印" min-width="90">
            <template #default="{ row }">{{ row.soulMark?.name || '—' }}</template>
          </el-table-column>
          <el-table-column label="印记" min-width="110">
            <template #default="{ row }">
              <span :class="{ mono: false }" style="font-size:12px;">{{ marksText(row) || '—' }}</span>
            </template>
          </el-table-column>
          <el-table-column label="异常" min-width="100">
            <template #default="{ row }">
              <span style="font-size:12px;">{{ abnormalText(row) || '—' }}</span>
            </template>
          </el-table-column>
        </el-table>
      </el-col>
    </el-row>

    <!-- 时点桶：分组折叠 -->
    <el-collapse>
      <el-collapse-item v-for="b in BUCKETS" :key="b.key"
                        :name="b.key"
                        :disabled="!bucketTotal(fs[b.key])">
        <template #title>
          <b>{{ b.title }}</b>&nbsp;
          <el-tag size="small" :type="bucketTotal(fs[b.key]) ? 'primary' : 'info'" effect="plain">
            {{ bucketTotal(fs[b.key]) }} 条
          </el-tag>
        </template>
        <div v-for="g in bucketGroups(fs[b.key])" :key="g.state + '_' + g.player" style="margin-bottom:8px;">
          <div class="mono" style="color:#909399; margin:4px 0;">{{ groupLabel(g) }}</div>
          <el-table :data="g.effects" size="small" border>
            <el-table-column label="效果" min-width="260">
              <template #default="{ row }">
                <div>{{ effectMainText(row, b) }}
                  <span v-if="soulNodeSuffix(row)" class="mono" style="color:#909399;">{{ soulNodeSuffix(row) }}</span>
                </div>
                <div class="mono" style="color:#c0c4cc;">
                  {{ effectFallback(row, b) }}<template v-if="row.once"> · 回合首刷新</template>
                </div>
              </template>
            </el-table-column>
            <el-table-column label="参数" width="90">
              <template #default="{ row }">
                <span class="mono">{{ (row.args || []).join(',') || '—' }}</span>
              </template>
            </el-table-column>
            <el-table-column label="来源" min-width="100">
              <template #default="{ row }">{{ effectSource(row, b) }}</template>
            </el-table-column>
            <el-table-column label="作用域" width="70">
              <template #default="{ row }">{{ scopeName(row.scope) }}</template>
            </el-table-column>
            <el-table-column label="状态" width="96">
              <template #default="{ row }">
                <el-tag size="small" :type="remainingTag(row, fs.round).type" effect="plain">
                  {{ remainingTag(row, fs.round).text }}
                </el-tag>
              </template>
            </el-table-column>
          </el-table>
        </div>
        <div v-if="!bucketTotal(fs[b.key])" class="mono" style="color:#c0c4cc;">（空）</div>
      </el-collapse-item>
    </el-collapse>

    <!-- 规则中心票 -->
    <el-collapse>
      <el-collapse-item name="tickets" :disabled="!(fs.ruleTickets || []).length">
        <template #title>
          <b>规则中心票</b>&nbsp;
          <el-tag size="small" :type="(fs.ruleTickets || []).length ? 'primary' : 'info'" effect="plain">
            {{ (fs.ruleTickets || []).length }} 张
          </el-tag>
        </template>
        <el-table v-if="(fs.ruleTickets || []).length" :data="fs.ruleTickets" size="small" border>
          <el-table-column label="类别" width="110">
            <template #default="{ row }">
              {{ ticketCategory(row) }}
              <div v-if="ticketSubtype(row)" style="font-size:12px; color:#909399;">{{ ticketSubtype(row) }}</div>
            </template>
          </el-table-column>
          <el-table-column label="票面文案" min-width="240">
            <template #default="{ row }">
              <div>{{ ticketText(row) }}</div>
              <div class="mono" style="color:#c0c4cc;">
                源 p{{ row.sourceOwner }}#{{ row.sourceSlot }} · {{ ticketPayload(row) }}
              </div>
            </template>
          </el-table-column>
          <el-table-column label="目标" width="60">
            <template #default="{ row }">p{{ row.target }}</template>
          </el-table-column>
          <el-table-column label="作用域" width="70">
            <template #default="{ row }">{{ scopeName(row.scope) }}</template>
          </el-table-column>
          <el-table-column label="状态" width="90">
            <template #default="{ row }">
              <el-tag size="small" :type="row.expired ? 'info' : 'success'" effect="plain">
                {{ row.expired ? '已过期' : '生效中' }}
              </el-tag>
            </template>
          </el-table-column>
        </el-table>
        <div v-else class="mono" style="color:#c0c4cc;">（无）</div>
      </el-collapse-item>
    </el-collapse>

    <!-- 结算视图 + 原始 JSON -->
    <el-collapse>
      <el-collapse-item name="ws">
        <template #title><b>结算视图（workspace）</b></template>
        <el-table :data="wsRows" size="small" border>
          <el-table-column label="项" width="120">
            <template #default="{ row }">{{ row[0] }}</template>
          </el-table-column>
          <el-table-column prop="1" label="值" min-width="300">
            <template #default="{ row }"><span class="mono">{{ row[1] }}</span></template>
          </el-table-column>
        </el-table>
      </el-collapse-item>
      <el-collapse-item name="raw" title="原始 JSON">
        <pre class="mono" style="max-height:40vh; overflow:auto; white-space:pre-wrap; margin:0;">{{ battle.fullstate || '（空）' }}</pre>
      </el-collapse-item>
    </el-collapse>
  </div>
  <div v-else class="mono" style="color:#909399;">（尚未请求）</div>
</template>

<style scoped>
:deep(.onstage-row) { background: #f0f9eb; }
:deep(.dead-row) { color: #c0c4cc; }
</style>
