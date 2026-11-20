<script setup>
import { ref, watch, computed } from 'vue'
import { ElMessage } from 'element-plus'
import { get } from '../api'
import { defaultCultivate } from '../store'

const props = defineProps({ slotData: { type: Object, required: true } })
const emit = defineEmits(['confirm'])

const visible = defineModel({ type: Boolean, default: false })

const cultivate = ref(defaultCultivate())

const q = ref('')
const results = ref([])
const searching = ref(false)
const detail = ref(null)
const regularSkills = ref([])   // 常规 4 槽
const fifthSkill = ref(0)       // 第五技能（0 = 未选）
const traitId = ref(0)
const traitQ = ref('')
const traits = ref([])
const base = ref([0, 0, 0, 0, 0, 0])
const oracleOn = ref(false)   // 神谕开关（神谕波次精灵）：选魂印前后版本 + 解锁神谕新技

const BASE_LABELS = ['攻击', '特攻', '防御', '特防', '速度', '体力']

let searchSeq = 0
async function search() {
  const seq = ++searchSeq
  searching.value = true
  try {
    const data = await get('/pets', { q: q.value, limit: 50 })
    if (seq === searchSeq) results.value = data.pets
  } finally {
    searching.value = false
  }
}
watch(q, () => search())
watch(visible, (v) => {
  if (v) {
    detail.value = props.slotData.detail
    oracleOn.value = !!props.slotData.oracle
    traitId.value = props.slotData.traitId || 0
    base.value = [...(props.slotData.base || [0, 0, 0, 0, 0, 0])]
    splitSkills(props.slotData.skills || [])
    // 培养（培养线）：整对象带回，换宠重开编辑不丢配置
    cultivate.value = props.slotData.cultivate
      ? JSON.parse(JSON.stringify(props.slotData.cultivate))
      : defaultCultivate()
    // 已有词条覆盖按技能分组回填（TeamPanel"覆盖"整份粘贴 / 重开编辑都走这里）
    paramEdits.value = {}
    for (const e of props.slotData.effectParams || []) {
      (paramEdits.value[e.skill] ||= []).push({ effectId: e.effect, index: e.index, value: e.value })
    }
    if (results.value.length === 0) search()
  }
})

/** 把已有 5 技能按第五候选（hide_moves）拆回 4+1。 */
function splitSkills(skills) {
  const fifthIds = fifthIdSet.value
  const regular = []
  let fifth = 0
  for (const id of skills) {
    if (fifthIds.has(id) && !fifth) fifth = id
    else regular.push(id)
  }
  regularSkills.value = regular
  fifthSkill.value = fifth
}

async function pickPet(row) {
  detail.value = await get(`/pets/${row.petId}`)
  regularSkills.value = []
  fifthSkill.value = 0
  oracleOn.value = false  // 官方默认 = 未神谕形态
  paramEdits.value = {}   // 换精灵 = 换技能池，旧覆盖全弃
  // 默认配招：有第五取最后一条（觉醒/神谕版在后），常规取推荐位(rec)；
  // 没有第五的精灵（如波克尔）直接任选 5 个
  const cap = hasFifth.value ? 4 : 5
  const sorted = [...detail.value.learnableMoves].sort(
    (a, b) => (b.rec || 0) - (a.rec || 0) || (a.learnLv || 0) - (b.learnLv || 0))
  const fifths = fifthPool.value
  if (fifths.length) fifthSkill.value = fifths[fifths.length - 1].id
  for (const m of sorted) {
    if (regularSkills.value.length >= cap) break
    if (m.id === fifthSkill.value) continue
    regularSkills.value.push(m.id)
  }
}

// 第五技能候选 = hide_moves 表（官方第五技能不走学习表）。
// 神谕波次精灵：OFF 剔除神谕新第五（引擎未神谕会锁）；ON 追加神谕第五候选。
const hasOracle = computed(() => !!detail.value?.oracle?.hasOracle)
const fifthPool = computed(() => {
  const d = detail.value
  if (!d) return []
  if (!d.oracle?.hasOracle) return d.fifthCandidates || []
  const onFifth = new Set(d.oracle.fifth || [])
  const base = (d.fifthCandidates || []).filter((m) => !onFifth.has(m.id))
  return oracleOn.value ? [...base, ...(d.oracleFifthCandidates || [])] : base
})
const normalPool = computed(() => {
  const d = detail.value
  if (!d) return []
  if (!d.oracle?.hasOracle) return d.learnableMoves || []
  const onNormal = new Set(d.oracle.moves || [])
  const base = (d.learnableMoves || []).filter((m) => !onNormal.has(m.id))
  return oracleOn.value ? [...base, ...(d.oracleMoves || [])] : base
})
const fifthIdSet = computed(() => new Set(fifthPool.value.map((m) => m.id)))
const hasFifth = computed(() => fifthPool.value.length > 0)

/** 神谕开关切换：修剪失效选择（不在当前池的技能移除）。 */
function toggleOracle(v) {
  oracleOn.value = v
  const ids = new Set(normalPool.value.map((m) => m.id))
  regularSkills.value = regularSkills.value.filter((id) => ids.has(id))
  if (fifthSkill.value && !fifthIdSet.value.has(fifthSkill.value)) fifthSkill.value = 0
}

/** 神谕前后魂印文本（多版本行按 icon id 升序 = 引擎序：低=前 / 高=后）。 */
const soulMarkVersions = computed(() => {
  const d = detail.value
  if (!d?.oracle?.soulMarkVersions) return null
  const marks = d.soulMarks || []
  if (marks.length < 2) return null
  return { before: marks[0], after: marks[marks.length - 1] }
})

// ── 实现明细弹窗："部分未实现"等徽标可点击，看具体哪个效果没实现 ──
// 2026-09-26 兼作"效果禁用"入口：每条效果带开关（禁用=装配期整体摘除，开局生效）。
const implDetail = ref(null)   // { skillName, loading, data }
const STATUS_TAG = { ok: 'success', partial: 'warning', none: 'danger',
                     ignored: 'info', plain: 'info' }
// boss 无效清单（网关 /api/boss-invalid-effects）：徽标 + "预填 boss 对局禁用"按钮
const bossInvalidIds = ref(new Set())
get('/boss-invalid-effects').then((d) => {
  bossInvalidIds.value = new Set(d.ids || [])
}).catch(() => {})
const isBossInvalid = (eid) => bossInvalidIds.value.has(eid)
// 禁用集（本弹窗内即时可见；确认时随 onConfirm 带出）
const disabledEffects = ref({ skillEffects: [], soulMarks: [] })
function toggleDisable(eid) {
  const arr = disabledEffects.value.skillEffects
  const i = arr.indexOf(eid)
  if (i >= 0) arr.splice(i, 1)
  else arr.push(eid)
}
// 魂印禁用开关：绑定装配期实际选行的 effect_id（soulMarks 尾行=生效行，K1 选行同源）
const effectiveSoulId = computed(() => {
  const marks = detail.value?.soulMarks || []
  return marks.length ? marks[marks.length - 1].effectId : 0
})
const disableSoul = computed({
  get: () => effectiveSoulId.value > 0
          && disabledEffects.value.soulMarks.includes(effectiveSoulId.value),
  set: (v) => {
    const m = effectiveSoulId.value
    if (m <= 0) return
    const i = disabledEffects.value.soulMarks.indexOf(m)
    if (v && i < 0) disabledEffects.value.soulMarks.push(m)
    if (!v && i >= 0) disabledEffects.value.soulMarks.splice(i, 1)
  },
})
async function openImplDetail(row) {
  implDetail.value = { skillName: row.name, loading: true, data: null }
  try {
    implDetail.value.data = await get(`/moves/${row.id}`)
  } catch {
    ElMessage.error('实现明细拉取失败')
    implDetail.value = null
    return
  }
  implDetail.value.loading = false
}
// boss 对局预填：把该技能引用的所有 boss 无效效果一键加入禁用集
function prefillBossInvalid() {
  const effs = implDetail.value?.data?.effects || []
  let added = 0
  for (const e of effs) {
    if (isBossInvalid(e.id) && !disabledEffects.value.skillEffects.includes(e.id)) {
      disabledEffects.value.skillEffects.push(e.id)
      ++added
    }
  }
  ElMessage.success(added ? `已禁用 ${added} 个 boss 无效效果` : '该技能没有 boss 无效效果')
}

function toggleSkill(id) {
  if (fifthIdSet.value.has(id)) {
    // 第五技能槽：单选、可换（原版/觉醒版点谁上谁）
    fifthSkill.value = fifthSkill.value === id ? 0 : id
    return
  }
  const i = regularSkills.value.indexOf(id)
  const cap = hasFifth.value ? 4 : 5
  if (i >= 0) regularSkills.value.splice(i, 1)
  else if (regularSkills.value.length < cap) regularSkills.value.push(id)
  else ElMessage.warning(hasFifth.value ? '常规技能最多 4 个（第五技能单独选）' : '最多选 5 个技能')
}

watch(traitQ, async (v) => {
  traits.value = (await get('/traits', { q: v, limit: 20 })).traits
})

const skillName = (id) =>
  allSkillRows.value.find((m) => m.id === id)?.name || `#${id}`

function confirm() {
  if (!detail.value) return ElMessage.warning('先选一只精灵')
  if (hasFifth.value) {
    if (regularSkills.value.length !== 4) return ElMessage.warning('常规技能需要恰好 4 个')
    if (!fifthSkill.value) return ElMessage.warning('还要选 1 个第五技能（多阶段点谁上谁）')
  } else {
    if (regularSkills.value.length !== 5) {
      return ElMessage.warning('该精灵没有第五技能标记，需要任选 5 个技能')
    }
  }
  const finalSkills = hasFifth.value
    ? [...regularSkills.value, fifthSkill.value]
    : [...regularSkills.value]
  // 词条覆盖只保留仍在最终配招里的技能（被取消选择的技能，覆盖一并弃）
  const effectParams = []
  for (const sid of finalSkills) {
    for (const e of paramEdits.value[sid] || []) {
      effectParams.push({ skill: sid, effect: e.effectId, index: e.index, value: e.value })
    }
  }
  // 效果禁用：魂印禁用直接带出；技能效果禁用只保留仍在最终配招里的
  const de = disabledEffects.value
  const skillEffects = de.skillEffects.filter((eid) =>
    finalSkills.some((sid) => {
      const rec = detail.value?.learnableMoves?.find((m) => m.id === sid)
      return rec?.effects?.some((e) => e.id === eid)
        || rec?.badge?.effects?.some((e) => e.id === eid)
    }))
  emit('confirm', {
    petId: detail.value.petId,
    name: detail.value.name,
    detail: detail.value,
    skills: finalSkills,
    oracle: oracleOn.value,
    traitId: traitId.value,
    base: [...base.value],
    effectParams,
    disabledEffects: { skillEffects, soulMarks: [...de.soulMarks] },
    cultivate: cultivate.value || defaultCultivate(),
  })
  visible.value = false
}

const statusClass = (s) => `badge-${s}`
const allSkillRows = computed(() => [...normalPool.value, ...fifthPool.value])

// ── 词条参数编辑（调试台"词条编辑"线）：paramEdits[skillId] = [{effectId,index,value}]，
// 只记与 DB 默认值不同的项；index = effect_info.info 模板 {n} 占位符下标 ──
const paramEdits = ref({})
const effDialog = ref(null)   // { skillId, skillName, loading, effects: [{effectId, template, argsNum, dbArgs, vals}] }

async function openEffectEditor(skillId) {
  effDialog.value = { skillId, skillName: skillName(skillId), loading: true, effects: [] }
  try {
    const data = await get(`/moves/${skillId}/effects`)
    const edited = new Map(
      (paramEdits.value[skillId] || []).map((e) => [`${e.effectId}:${e.index}`, e.value]))
    effDialog.value.effects = (data.effects || []).map((e) => {
      const dbArgs = Array.from({ length: e.argsNum || 0 }, (_, i) => e.args?.[i] ?? 0)
      return {
        effectId: e.effectId,
        template: e.template || '',
        argsNum: e.argsNum || 0,
        dbArgs,
        vals: dbArgs.map((v, i) => edited.get(`${e.effectId}:${i}`) ?? v),
      }
    })
  } catch {
    ElMessage.error('词条元数据拉取失败')
    effDialog.value = null
    return
  }
  effDialog.value.loading = false
}

/** 模板 "{0}%令对手{1}" → [{t:'txt',v}, {t:'arg',v:下标}, ...] */
function tplParts(tpl) {
  const s = String(tpl || '')
  const parts = []
  let last = 0
  s.replace(/\{(\d+)\}/g, (m, n, off) => {
    if (off > last) parts.push({ t: 'txt', v: s.slice(last, off) })
    parts.push({ t: 'arg', v: Number(n) })
    last = off + m.length
    return m
  })
  if (last < s.length) parts.push({ t: 'txt', v: s.slice(last) })
  return parts
}

function saveEffectEdits() {
  const d = effDialog.value
  if (!d) return
  const entries = []
  for (const e of d.effects) {
    for (let i = 0; i < e.vals.length; ++i) {
      if (e.vals[i] !== e.dbArgs[i]) {
        entries.push({ effectId: e.effectId, index: i, value: e.vals[i] })
      }
    }
  }
  if (entries.length) paramEdits.value[d.skillId] = entries
  else delete paramEdits.value[d.skillId]
  effDialog.value = null
}
const editCount = (sid) => (paramEdits.value[sid] || []).length
</script>

<template>
  <el-dialog v-model="visible" title="配置出战精灵" width="1020px" top="4vh">
    <div style="display:flex; gap:14px;">
      <!-- 左：搜索选宠 -->
      <div style="flex: 0 0 320px; display:flex; flex-direction:column; gap:8px;">
        <el-input v-model="q" placeholder="搜名字或图鉴号，如 波克尔 / 12" clearable />
        <el-table :data="results" height="430" size="small" highlight-current-row
                  @row-click="pickPet" v-loading="searching">
          <el-table-column prop="petId" label="图鉴号" width="80" />
          <el-table-column prop="name" label="精灵" min-width="110" />
          <el-table-column prop="element" label="属性" width="110" />
          <el-table-column label="数据" width="90">
            <template #default="{ row }">
              <el-tag v-if="row.has4399" size="small" type="success">4399</el-tag>
              <el-tag v-else size="small" type="info">库</el-tag>
              <el-tag v-if="!row.teamable" size="small" type="danger" style="margin-left:2px">不可组</el-tag>
            </template>
          </el-table-column>
        </el-table>
      </div>

      <!-- 右：技能 / 数值 / 特性 -->
      <div style="flex:1; min-width:0;">
        <template v-if="detail">
          <div style="margin-bottom:6px;">
            <b style="font-size:16px;">{{ detail.name }}</b>
            <el-tag size="small" style="margin-left:6px;">#{{ detail.petId }}</el-tag>
            <el-tag size="small" type="warning" style="margin-left:4px;">{{ detail.element }}</el-tag>
          </div>

          <!-- 神谕开关（神谕波次精灵）：魂印前/后选行 + 解锁神谕新技 -->
          <div v-if="hasOracle" style="display:flex; align-items:center; gap:8px; margin-bottom:6px;">
            <el-switch :model-value="oracleOn" @change="(v) => toggleOracle(v)" />
            <b :style="{ color: oracleOn ? '#e6a23c' : '#909399' }">
              {{ oracleOn ? '神谕已开启' : '神谕未开启' }}
            </b>
            <span style="color:#909399; font-size:12px;">
              切换魂印前/后版本并解锁神谕新技（技能池随开关变化，失效选择自动修剪）
            </span>
          </div>
          <!-- 魂印实现状况（整条粒度：registry.soul 有注册 = 已实现，程序含多子句）+ 禁用开关 -->
          <div style="display:flex; gap:6px; margin-bottom:6px; align-items:stretch;">
            <div v-for="(m, mi) in detail.soulMarks || []" :key="mi" class="sm-box"
                 :class="{ 'sm-active': soulMarkVersions
                     ? (oracleOn ? mi === (detail.soulMarks.length - 1) : mi === 0)
                     : true }">
              <div class="sm-title">
                魂印{{ soulMarkVersions ? (mi === 0 ? '（未神谕）' : '（神谕）') : '' }}
                <el-tag size="small"
                        :type="(m.badge?.status === 'ok') ? 'success' : 'danger'"
                        style="margin-left:4px;">
                  {{ (m.badge?.status === 'ok') ? '已实现' : '未实现' }}
                </el-tag>
              </div>
              <div class="sm-text">{{ m.tips || '（无文本）' }}</div>
            </div>
          </div>

          <!-- 魂印禁用（2026-09-26 禁用基建）：装配期摘除整个魂印程序 → 裸宠 -->
          <div style="display:flex; align-items:center; gap:8px; margin-bottom:6px;">
            <el-switch v-model="disableSoul" inline-prompt
                       active-text="魂印禁用" inactive-text="魂印启用" size="small" />
            <span style="color:#909399; font-size:12px;">
              禁用后该精灵以裸魂印出战（效果不注册不触发；boss 对局"boss无效"魂印条款用）
            </span>
          </div>

          <!-- 当前选择概览 -->
          <div style="display:flex; gap:4px; flex-wrap:wrap; margin-bottom:6px;">
            <el-tag v-for="(sid, i) in regularSkills" :key="sid" size="small" effect="plain">
              {{ i + 1 }}. {{ skillName(sid) }}
            </el-tag>
            <el-tag v-if="fifthSkill" size="small" type="warning">第五. {{ skillName(fifthSkill) }}</el-tag>
          </div>

          <div class="section-title">
            技能（常规 {{ hasFifth ? 4 : 5 }} + 第五 {{ hasFifth ? 1 : 0 }}
            <template v-if="hasFifth">；第五=图鉴隐藏技能，多候选点谁上谁）</template>
            <template v-else>；该精灵没有第五技能，任选即可）</template>
          </div>
          <el-table :data="allSkillRows" height="290" size="small"
                    @row-click="(row) => toggleSkill(row.id)">
            <el-table-column width="96">
              <template #default="{ row }">
                <el-radio v-if="fifthIdSet.has(row.id)"
                          :model-value="fifthSkill" :value="row.id"
                          @click.stop @change="() => toggleSkill(row.id)">第五</el-radio>
                <el-checkbox v-else :model-value="regularSkills.includes(row.id)"
                             @change="() => toggleSkill(row.id)" @click.stop />
              </template>
            </el-table-column>
            <el-table-column prop="name" label="技能" width="150">
              <template #default="{ row }">
                {{ row.name }}
                <el-tag v-if="fifthIdSet.has(row.id)" size="small" type="warning" effect="plain">第五</el-tag>
              </template>
            </el-table-column>
            <el-table-column prop="element" label="属性" width="64" />
            <el-table-column prop="category" label="类别" width="64" />
            <el-table-column prop="power" label="威力" width="60" />
            <el-table-column label="先制" width="52">
              <template #default="{ row }">
                <span v-if="row.priority" class="mono"
                      :style="{ color: row.priority > 0 ? '#67c23a' : '#f56c6c' }">
                  {{ row.priority > 0 ? '+' + row.priority : row.priority }}
                </span>
                <span v-else style="color:#556;">0</span>
              </template>
            </el-table-column>
            <el-table-column prop="pp" label="PP" width="52" />
            <el-table-column label="效果实现" width="110">
              <template #default="{ row }">
                <el-tooltip :disabled="row.badge.status === 'ok' || row.badge.status === 'plain'"
                            content="点击查看逐条实现明细（哪个效果没实现 + 描述）">
                  <span class="impl-link" @click.stop="openImplDetail(row)">
                    <span :class="statusClass(row.badge.status)" class="badge-dot" />
                    {{ row.badgeLabel }}
                  </span>
                </el-tooltip>
              </template>
            </el-table-column>
            <el-table-column label="词条" width="86">
              <template #default="{ row }">
                <el-button size="small" text type="primary" @click.stop="openEffectEditor(row.id)">
                  参数{{ editCount(row.id) ? `·${editCount(row.id)}` : '' }}
                </el-button>
              </template>
            </el-table-column>
            <el-table-column label="效果说明" min-width="200">
              <template #default="{ row }">
                <el-tooltip placement="top" :content="row.info || '—'">
                  <span>{{ (row.info || '—').slice(0, 26) }}{{ (row.info || '').length > 26 ? '…' : '' }}</span>
                </el-tooltip>
              </template>
            </el-table-column>
          </el-table>

          <div class="section-title">自定义数值（0 = 用库值；攻/特攻/防/特防/速/体力）</div>
          <div style="display:flex; gap:6px; flex-wrap:wrap;">
            <div v-for="(label, i) in BASE_LABELS" :key="label" style="text-align:center;">
              <div class="mono" style="color:#909399;">{{ label }}</div>
              <el-input-number v-model="base[i]" :min="0" :max="9999" size="small"
                               controls-position="right" style="width:92px;" />
            </div>
          </div>

          <div class="section-title">通用特性（可选）</div>
          <el-select v-model="traitId" filterable clearable remote placeholder="搜特性（如 叶绿）"
                     :remote-method="(v) => (traitQ = v)" style="width:100%;">
            <el-option v-for="t in traits" :key="t.idx" :value="t.idx"
                       :label="`${t.idx} ${t.name} ${t.intro || ''}`" />
          </el-select>

          <div style="margin-top:10px; text-align:right;">
            <el-button @click="visible = false">取消</el-button>
            <el-button type="primary" @click="confirm">确认上场</el-button>
          </div>
        </template>
        <el-empty v-else description="左侧点一只精灵开始配置" />
      </div>
    </div>

    <!-- 词条参数编辑（嵌套弹窗）：模板 {n} 占位符 ↔ 参数下标，只记与库值不同的项 -->
    <el-dialog :model-value="!!effDialog" append-to-body width="760px" top="8vh"
               :title="`词条编辑 · ${effDialog?.skillName || ''}`"
               @update:model-value="(v) => (!v) && (effDialog = null)">
      <div v-if="effDialog" v-loading="effDialog.loading" style="min-height:120px;">
        <div v-for="e in effDialog.effects" :key="e.effectId" class="eff-card">
          <div class="mono" style="color:#909399; margin-bottom:6px;">
            效果 #{{ e.effectId }}
            <el-tag size="small" type="info" effect="plain">参数 {{ e.argsNum }}</el-tag>
          </div>
          <div v-if="e.argsNum > 0" class="eff-line">
            <template v-for="(p, pi) in tplParts(e.template)" :key="pi">
              <span v-if="p.t === 'txt'">{{ p.v }}</span>
              <el-input-number v-else v-model="e.vals[p.v]" size="small"
                               controls-position="right" style="width:92px; margin:0 3px;" />
            </template>
            <template v-if="!e.template">
              <span style="color:#909399;">（无模板文本）参数：</span>
              <el-input-number v-for="(v, i) in e.vals" :key="'p' + i" v-model="e.vals[i]"
                               size="small" controls-position="right"
                               style="width:92px; margin:0 3px;" />
            </template>
          </div>
          <div v-else style="color:#a0a6ad;">该效果无参数（args_num=0），行为在实现里定</div>
          <div v-if="e.argsNum > 0" style="color:#a0a6ad; font-size:12px; margin-top:4px;">
            库值：[{{ e.dbArgs.join(', ') }}]；留原值 = 不产生覆盖
          </div>
        </div>
        <div v-if="!effDialog.effects.length && !effDialog.loading" style="color:#a0a6ad;">
          该技能没有带参数的效果
        </div>
      </div>
      <template #footer>
        <el-button @click="effDialog = null">取消</el-button>
        <el-button type="primary" @click="saveEffectEdits">保存覆盖</el-button>
      </template>
    </el-dialog>

    <!-- 效果实现明细：逐条列出该技能引用的效果与实现状态 -->
    <el-dialog :model-value="!!implDetail" append-to-body width="720px" top="8vh"
               :title="`效果实现明细 · ${implDetail?.skillName || ''}`"
               @update:model-value="(v) => (!v) && (implDetail = null)">
      <div v-if="implDetail" v-loading="implDetail.loading" style="min-height:80px;">
        <div style="margin-bottom:8px; display:flex; align-items:center; gap:8px;">
          <el-tag size="small" type="info">禁用 = 装配期整体摘除（开局定死，重开生效）</el-tag>
          <el-button size="small" type="warning" plain @click="prefillBossInvalid">
            一键禁用 boss 无效效果
          </el-button>
        </div>
        <div v-for="e in implDetail.data?.effects || []" :key="e.id" class="eff-card">
          <div style="margin-bottom:4px; display:flex; align-items:center; gap:8px;">
            <b class="mono">效果 #{{ e.id }}</b>
            <el-tag size="small" :type="STATUS_TAG[e.status] || 'info'">{{ e.statusLabel }}</el-tag>
            <el-tag v-if="isBossInvalid(e.id)" size="small" type="warning">boss无效</el-tag>
            <el-switch
              :model-value="disabledEffects.skillEffects.includes(e.id)"
              @update:model-value="() => toggleDisable(e.id)"
              inline-prompt active-text="禁用" inactive-text="启用"
              size="small" style="margin-left:auto;" />
          </div>
          <div style="line-height:1.6;">{{ e.info || '（无模板文本）' }}</div>
        </div>
        <div v-if="implDetail.data && !(implDetail.data.effects || []).length"
             style="color:#a0a6ad;">该技能没有引用带 id 的效果</div>
      </div>
    </el-dialog>
  </el-dialog>
</template>

<style scoped>
.eff-card {
  border: 1px solid #e4e7ed; border-radius: 6px;
  padding: 8px 12px; margin-bottom: 8px;
}
.eff-line { line-height: 2; }
.sm-box {
  flex: 1; min-width: 0; border: 1px solid #e4e7ed; border-radius: 6px;
  padding: 6px 10px; opacity: .55;
}
.sm-box.sm-active { border-color: #e6a23c; opacity: 1; }
.sm-title { font-size: 12px; color: #909399; margin-bottom: 2px; }
.sm-text {
  font-size: 12px; line-height: 1.5;
  display: -webkit-box; -webkit-line-clamp: 3; -webkit-box-orient: vertical;
  overflow: hidden;
}
.impl-link { cursor: pointer; }
.impl-link:hover { color: var(--el-color-primary); text-decoration: underline; }
</style>
