<script setup>
import { ref, computed } from 'vue'
import { battle, submitAction, setStepMode, setAuto, applyBreakpoints,
         ABILITY_SLOTS, meta, submitNone, sendMutate } from '../store'
import { effectIndex, loadEffectIndex } from '../effectText'
import FullStatePanel from './FullStatePanel.vue'

const fullstateVisible = ref(false)
const bpSelection = ref([...battle.breakpoints])

// ── 手术台（二期）：白名单操作注入，泊车时由服务端执行 ──
const surgeryVisible = ref(false)
const SIDE_OPTS = [{ value: 0, label: '我方' }, { value: 1, label: '对方' }]
const SLOT_OPTS = Array.from({ length: 6 }, (_, i) => ({ value: i, label: `${i + 1}号位` }))
const SKILL_OPTS = Array.from({ length: 5 }, (_, i) => ({ value: i, label: `技能${i + 1}` }))
const STAT_OPTS = ABILITY_SLOTS.map((l, i) => ({ value: i, label: l }))
const anomalyOpts = computed(() =>
  meta.anomalies.map((a) => ({ value: a.id, label: a.name })))

// ── 技能不可选原因 → 展示（用户 2026-09-28：locked 与 PP 不足要视觉可分）──
const REASON_LABEL = {
  locked: '已封锁（未解封/被移除），无法选择',
  pp_empty: 'PP 已耗尽',
  unusable: '当前不可用',
  must_choose_pet: '需先选择出战精灵',
}
function skillButtonType(s) {
  if (s.usable) return 'primary'
  if (s.reason === 'locked') return 'danger'   // 封锁：红——不是 PP 的原因
  return 'info'                                 // 其余（含 PP 耗尽）：灰
}
function skillReasonText(s) {
  return REASON_LABEL[s.reason] || s.reason || ''
}
const OP_DEFS = {
  set_hp:    { label: '改体力',     fields: ['side', 'slot', 'value'],
               hint: 'hp=0 是裸状态，不走死亡结算（免死/复活/击败时点不触发）' },
  set_pp:    { label: '改PP',       fields: ['side', 'slot', 'skill', 'value'] },
  set_level: { label: '改等级',     fields: ['side', 'stat', 'value'] },
  anomaly:   { label: '上异常',     fields: ['side', 'id', 'rounds'],
               hint: '绕过免疫/抗性/转化直写权威存储' },
  cure:      { label: '解异常',     fields: ['side', 'id'] },
  mark:      { label: '挂死亡印记', fields: ['src', 'dst'],
               hint: '双方取当前在场位；对方行动结束后消耗全部体力' },
  unmark:    { label: '清死亡印记', fields: ['side'] },
}
const FIELD_DEFS = {
  side:   { key: 'side', type: 'select', options: SIDE_OPTS, dflt: 0 },
  src:    { key: 'src', type: 'select', options: SIDE_OPTS, dflt: 0 },
  dst:    { key: 'dst', type: 'select', options: SIDE_OPTS, dflt: 1 },
  slot:   { key: 'slot', type: 'select', options: SLOT_OPTS, dflt: 0 },
  skill:  { key: 'skill', type: 'select', options: SKILL_OPTS, dflt: 0 },
  stat:   { key: 'stat', type: 'select', options: STAT_OPTS, dflt: 0 },
  id:     { key: 'id', type: 'select',
            options: () => [{ value: -1, label: '全部' }, ...anomalyOpts.value], dflt: 7 },
  rounds: { key: 'rounds', type: 'number', dflt: 3 },
  value:  { key: 'value', type: 'number', dflt: 0 },
}
const surgeryOp = ref('anomaly')
const surgeryRow = ref({})
function switchOp(op) {
  surgeryOp.value = op
  const row = { op }
  for (const fk of OP_DEFS[op].fields) row[fk] = FIELD_DEFS[fk].dflt
  surgeryRow.value = row
}
switchOp('anomaly')
const surgeryFields = computed(() =>
  (OP_DEFS[surgeryOp.value]?.fields || []).map((fk) => {
    const def = FIELD_DEFS[fk]
    return { ...def, options: typeof def.options === 'function' ? def.options() : def.options }
  }))
function runSurgery() {
  const wire = { op: surgeryOp.value }
  for (const fk of OP_DEFS[surgeryOp.value].fields) wire[fk] = surgeryRow.value[fk]
  sendMutate([wire])
}

const waitingPlayer = computed(() =>
  battle.phase === 'running' && battle.input ? battle.input.player : null)

const sidePanels = computed(() => [
  { side: 0, label: '🟦 我方（玩家 0 · 房主）', snap: battle.snapshot?.player0,
    legal: battle.legals[0] },
  { side: 1, label: '🟥 对方（玩家 1）', snap: battle.snapshot?.player1,
    legal: battle.legals[1] },
])

// 预提交视角（没轮到也给动作清单）：按钮按本身门槛可用，提交后由服务端排队
const isProspective = (panel) => !!panel.legal?.prospective && waitingPlayer.value !== panel.side

function hpColor(p) {
  const ratio = p.maxHp ? p.hp / p.maxHp : 0
  return ratio > 0.5 ? '#67c23a' : ratio > 0.25 ? '#e6a23c' : '#f56c6c'
}
function hpPct(p) {
  return Math.max(0, Math.min(100, p.maxHp ? (p.hp / p.maxHp) * 100 : 0))
}

// 对战动画（2026-10-04）：fx 队列由 store.applyFx 在每批 TAPE 到达时聚合填充，
// 这里只负责展示。hit → 卡片抖动；death → 💀 遮罩；其余浮动数字见 .fx-item 样式。
function hasFx(side, type) {
  return battle.fx[side].some((f) => f.type === type)
}

function onBreakpointsChange(ids) {
  applyBreakpoints([...ids])
  bpSelection.value = [...battle.breakpoints]
}

// 全量状态：拉最新数据 + 效果显示索引（一次性，失败退化为裸 id 显示）
function openFullstate() {
  loadEffectIndex()
  battle.socket?.fullstate()
  fullstateVisible.value = true
}
</script>

<template>
  <div>
    <el-row :gutter="10">
      <el-col v-for="panel in sidePanels" :key="panel.side" :span="12">
        <el-card shadow="never"
                 :body-style="{ padding: '10px 12px', position: 'relative' }"
                 :class="{ 'fx-shake': hasFx(panel.side, 'hit') }"
                 :style="waitingPlayer === panel.side ? 'border: 1.5px solid #e6a23c;' : ''">
          <template #header>
            <div style="display:flex; align-items:center; gap:8px; padding:0;">
              <b>{{ panel.label }}</b>
              <el-tag v-if="waitingPlayer === panel.side" size="small" type="warning">等待操作</el-tag>
              <el-tag v-else-if="isProspective(panel)" size="small" type="info">预提交</el-tag>
              <el-tag v-if="battle.pending[panel.side]" size="small" type="success">已提交待结算</el-tag>
              <span style="margin-left:auto;">
                自动 <el-switch :model-value="battle.auto[panel.side]"
                               @update:model-value="(v) => setAuto(panel.side, v)" />
              </span>
            </div>
          </template>

          <!-- 出战精灵：血量 / 护盾 / 异常 / 能力等级 -->
          <template v-if="panel.snap?.pet">
            <div style="display:flex; align-items:center; gap:6px; flex-wrap:wrap;">
              <b>▶ {{ panel.snap.pet.name }}</b>
              <span class="mono">{{ panel.snap.pet.hp }}/{{ panel.snap.pet.maxHp }}</span>
              <el-tag v-if="panel.snap.pet.shield > 0" size="small" type="warning" effect="plain">
                🛡 盾 {{ panel.snap.pet.shield }}
              </el-tag>
              <el-tag v-for="ab in panel.snap.abnormalStates || []" :key="ab.id"
                      size="small" type="danger" effect="plain">
                {{ ab.name }} · 剩{{ ab.remaining }}回合
              </el-tag>
              <template v-for="(lv, i) in (panel.snap.pet.levels || [])" :key="i">
                <el-tag v-if="lv !== 0" size="small"
                        :type="lv > 0 ? 'success' : 'info'" effect="plain">
                  {{ ABILITY_SLOTS[i] }}{{ lv > 0 ? '+' + lv : lv }}
                </el-tag>
              </template>
            </div>
            <!-- 血条（宽度过渡 = 掉血/回血动画的主体） -->
            <div class="hp-bar">
              <div class="hp-bar-fill"
                   :style="{ width: hpPct(panel.snap.pet) + '%', background: hpColor(panel.snap.pet) }" />
            </div>
          </template>

          <!-- 技能（含剩余 PP） -->
          <div style="margin-top:8px; display:flex; flex-direction:column; gap:4px;">
            <el-tooltip v-for="s in panel.legal?.skills || []" :key="s.index"
                        :content="skillReasonText(s)" :disabled="s.usable || !s.reason"
                        placement="left">
            <el-button size="small"
                       :type="skillButtonType(s)"
                       :disabled="!s.usable || battle.phase !== 'running'"
                       style="justify-content:space-between; width:100%; margin-left:0;"
                       @click="submitAction(panel.side, 1, s.index)">
              <span>{{ s.name }}<el-tag v-if="!s.usable && s.reason === 'locked'" size="small"
                        type="danger" effect="plain" style="margin-left:6px;">封锁</el-tag></span>
              <span class="mono" style="margin-left:auto;">
                PP {{ s.pp < 0 ? '∞' : s.pp }}/{{ s.maxPp < 0 ? '∞' : s.maxPp }}
              </span>
            </el-button>
            </el-tooltip>
            <div v-if="!panel.legal" class="mono" style="color:#909399;">等待合法动作集…</div>
          </div>

          <!-- 换宠 -->
          <div style="margin-top:8px;">
            <span style="color:#909399; font-size:12px;">换宠：</span>
            <el-button v-for="p in panel.legal?.pets || []" :key="p.slot" size="small"
                       :disabled="!p.usable || p.onStage || battle.phase !== 'running'"
                       @click="submitAction(panel.side, 0, p.slot)">
              {{ p.name }} {{ p.hp }}/{{ p.maxHp }}
            </el-button>
          </div>

          <!-- 药剂 -->
          <div style="margin-top:6px;">
            <span style="color:#909399; font-size:12px;">药剂：</span>
            <el-button v-for="m in (panel.legal?.medicines || []).filter((x) => x.count > 0)"
                       :key="m.index" size="small"
                       :disabled="!m.usable || battle.phase !== 'running'"
                       @click="submitAction(panel.side, 2, m.index)">
              {{ m.name || ('药剂#' + m.itemId) }} ×{{ m.count }}
            </el-button>
            <span v-if="!(panel.legal?.medicines || []).some((x) => x.count > 0)"
                  class="mono" style="color:#c0c4cc;">无</span>
          </div>

          <!-- 空操作：超时什么都不做，回合类/异常后续照常。
               死切期（canSkip=false）不提供——FSM 只收换宠，D1 口径从源头不给按钮。 -->
          <div v-if="!panel.legal || panel.legal.canSkip !== false" style="margin-top:8px;">
            <el-button size="small" :disabled="battle.phase !== 'running'"
                       @click="submitNone(panel.side)">⏭ 空操作（超时跳过）</el-button>
          </div>

          <!-- 对战动画浮层：伤害数字/治疗/能力变化/破盾（store.fx 驱动，纯展示） -->
          <div class="fx-layer" aria-hidden="true">
            <div v-for="(f, i) in battle.fx[panel.side]" :key="f.id"
                 class="fx-item" :class="'fx-' + f.type"
                 :style="{ animationDelay: (i * 110) + 'ms' }">{{ f.text }}</div>
          </div>
          <div v-if="hasFx(panel.side, 'death')" class="fx-death-veil" aria-hidden="true">💀</div>
        </el-card>
      </el-col>
    </el-row>

    <!-- 调试控制 -->
    <el-card shadow="never" style="margin-top:10px;" :body-style="{ padding: '8px 12px' }">
      <div style="display:flex; gap:14px; align-items:center; flex-wrap:wrap;">
        <div>
          逐步模式
          <el-switch :model-value="battle.stepMode"
                     @update:model-value="(v) => setStepMode(v)" />
        </div>
        <el-button size="small" :disabled="!battle.stepMode || battle.phase !== 'running'"
                   @click="battle.socket?.step()">推进一个时点</el-button>
        <el-button size="small" :disabled="battle.phase !== 'running'"
                   @click="openFullstate()">全量状态</el-button>
        <el-button size="small" :disabled="battle.phase !== 'running'"
                   @click="surgeryVisible = true">🔧 手术台</el-button>
        <span class="mono" style="color:#909399; display:inline-flex; align-items:center; gap:4px;">
          断点：
          <el-select v-model="bpSelection" multiple filterable placeholder="选时点（可搜中文名）"
                     style="min-width:300px;" size="small" :disabled="battle.phase !== 'running'"
                     @change="onBreakpointsChange">
            <el-option v-for="s in meta.states" :key="s.id" :value="s.id"
                       :label="`时点${s.id} ${s.name}`" />
          </el-select>
        </span>
      </div>
    </el-card>

    <el-dialog v-model="fullstateVisible" title="全量状态（效果/印记/规则票）" width="900px" top="4vh">
      <div v-if="!effectIndex.loaded" class="mono" style="color:#e6a23c; margin-bottom:6px;">
        效果显示索引未就绪（网关 /api/effect-index 未拉到），效果按裸 id 显示
      </div>
      <FullStatePanel />
    </el-dialog>

    <!-- 手术台：白名单操作注入，泊车时执行，回执进事件带 -->
    <el-dialog v-model="surgeryVisible" title="🔧 手术台（运行时状态注入）" width="660px" append-to-body>
      <el-alert type="info" :closable="false" style="margin-bottom:10px;"
                title="仅引擎泊车时可用（等输入 / 逐步模式停下 / 断点命中）；每次执行后自动刷新状态与合法动作集，回执显示在事件带（🔧）。" />
      <div style="display:flex; gap:8px; align-items:center; flex-wrap:wrap;">
        <el-select v-model="surgeryOp" style="width:140px" @change="switchOp">
          <el-option v-for="(def, op) in OP_DEFS" :key="op" :value="op" :label="def.label" />
        </el-select>
        <template v-for="f in surgeryFields" :key="f.key">
          <el-select v-if="f.type === 'select'" v-model="surgeryRow[f.key]"
                     style="width:110px" size="small">
            <el-option v-for="opt in f.options" :key="opt.value" :value="opt.value" :label="opt.label" />
          </el-select>
          <el-input-number v-else v-model="surgeryRow[f.key]" size="small"
                           controls-position="right" style="width:104px;" />
        </template>
        <el-button type="primary" size="small" @click="runSurgery">执行</el-button>
      </div>
      <div v-if="OP_DEFS[surgeryOp]?.hint" style="color:#a0a6ad; font-size:12px; margin-top:8px;">
        {{ OP_DEFS[surgeryOp].hint }}
      </div>
    </el-dialog>
  </div>
</template>
