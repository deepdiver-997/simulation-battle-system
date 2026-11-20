<script setup>
// 培养编辑器（培养线 feat/peiyang，2026-09-27）：学习力/性格/刻印/账号级固定加成
// → 实时预览合成面板（/api/cultivation/preview，引擎公式的网关镜像）。
// 引擎为权威：对局内面板由 sim_server 装配期合成，本编辑器只产出协议字段 + 预览。
import { ref, reactive, watch, computed } from 'vue'
import { ElMessage } from 'element-plus'
import { get, postJson } from '../api'
import { defaultCultivate, cultivateMeta, loadCultivateMeta, anomalyName, meta, team } from '../store'

const props = defineProps({
  slotData: { type: Object, required: true },   // TeamPanel 槽位（读写 .cultivate）
})
const visible = defineModel({ type: Boolean, default: false })

const cul = reactive(defaultCultivate())
const preview = ref(null)        // {ok, errors, race, stats, breakdown}
const previewing = ref(false)
let previewTimer = null

const STAT_LABELS = ['攻击', '特攻', '防御', '特防', '速度', '体力']
const activeView = ref('base')   // 预览三视角：base=双场景有效 / pvp / pve（有装备限定时有差异）
const EV_TOTAL_CAP = 510
const evTotal = computed(() => cul.ev.reduce((a, b) => a + (b || 0), 0))
const viewStats = computed(() =>
  (activeView.value === 'base' ? preview.value?.stats : preview.value?.views?.[activeView.value])
  || preview.value?.stats || null)
const evOver = computed(() => cul.ev.some((e) => e > 255) || evTotal.value > EV_TOTAL_CAP)

// 三个刻印槽的远程候选。⚠️ 必须是 reactive 数组而非 [ref, ref, ref]：
// 模板 `keryinOptions[i]` 对嵌套 ref 不解包，v-for 会去迭代 ref 对象自己的
// 内部属性——实测渲染出 5 个空 option（2026-09-27"控制台搜不到任何刻印"的根因）。
const keryinOptions = reactive([[], [], []])
const keryinLoading = ref(false)

watch(visible, async (v) => {
  if (!v) return
  // 整对象深拷贝入编辑器（确认才写回槽位，取消不伤原配置）
  Object.assign(cul, JSON.parse(JSON.stringify(
    props.slotData.cultivate || defaultCultivate())))
  // 旧培养配置无 titleId 字段 → 显式归 0（防上一只精灵的编辑态残留）
  cul.titleId = props.slotData.cultivate?.titleId ?? 0
  cul.resist = JSON.parse(JSON.stringify(
    props.slotData.cultivate?.resist || defaultCultivate().resist))
  await loadCultivateMeta()
  schedulePreview()
  for (let i = 0; i < 3; ++i) {
    if (cul.inscriptions[i] > 0) searchKeryin('', i, cul.inscriptions[i])
  }
})

// 配置变化 → 300ms 防抖预览
watch(cul, () => schedulePreview(), { deep: true })
function schedulePreview() {
  clearTimeout(previewTimer)
  previewTimer = setTimeout(doPreview, 300)
}
async function doPreview() {
  if (!props.slotData.petId) return
  previewing.value = true
  try {
    preview.value = await postJson('/cultivation/preview', {
      petId: props.slotData.petId,
      ev: [...cul.ev],
      natureId: cul.natureId,
      titleId: cul.titleId,
      inscriptions: [...cul.inscriptions],
      hpCap20: cul.hpCap20,
      guild: cul.guild,
      annual: cul.annual,
      titleId: cul.titleId,
      equip: [...(team.equip.side1 || [])],
    })
  } catch (e) {
    preview.value = { ok: false, errors: [String(e.message || e)] }
  } finally {
    previewing.value = false
  }
}

// ── 刻印远程搜索：q 空 + 有已选 id 时走 id 精确回显 ──
async function searchKeryin(q, slot, echoId = 0) {
  keryinLoading.value = true
  try {
    const params = { q, limit: 30 }
    const data = await get('/cultivation/inscriptions', params)
    let list = data.inscriptions || []
    if (echoId > 0 && !list.some((k) => k.id === echoId)) {
      const byId = await get('/cultivation/inscriptions', { q: String(echoId), limit: 5 })
      list = [...list, ...(byId.inscriptions || []).filter((k) => k.id === echoId)]
    }
    keryinOptions[slot] = list
  } catch { /* 网关未起 */ } finally {
    keryinLoading.value = false
  }
}
function onKeryinSearch(q, slot) { searchKeryin(q, slot) }

// 称号下拉：名称 + 六维加成文本（引擎序：攻 特攻 防 特防 速 体）+ 专属标注。
const STAT_KEYS = ['攻', '特攻', '防', '特防', '速', '体']
const titleStatText = (t) => {
  return t.stats
    .map((v, i) => (v ? `${STAT_KEYS[i]}+${v}` : ''))
    .filter(Boolean)
    .join(' ') || '无加成'
}
const titleLabel = (t) => `${t.name}｜${titleStatText(t)}`

const keryinLabel = (k) => {
  const ex = k.exclusiveNames?.length ? `（专属:${k.exclusiveNames.join('/')}）` : ''
  return `${k.name}｜${k.effectDes}${ex}`
}

function confirm() {
  if (cul.enabled && evOver.value) {
    ElMessage.warning('学习力超限：单项 ≤255、总和 ≤510')
    return
  }
  // 写回槽位（cultivate 是深拷贝对象，直接整体替换）
  props.slotData.cultivate = JSON.parse(JSON.stringify(cul))
  ElMessage.success(cul.enabled ? '培养配置已应用（对局面板由引擎合成）' : '已关闭培养（回退直传数值）')
  visible.value = false
}
</script>

<template>
  <el-dialog v-model="visible" title="培养配置" width="640px" append-to-body>
    <div v-if="!slotData.petId" style="color:#a0a6ad;">先选精灵再配培养</div>
    <template v-else>
      <div style="display:flex; align-items:center; gap:10px; margin-bottom:10px;">
        <el-switch v-model="cul.enabled" />
        <b>启用培养（拟真合成）</b>
        <span style="color:#a0a6ad; font-size:12px;">
          面板 =（种族×2 + 学习力÷4 + 天赋31）×性格 + 刻印 + 体力上限 + 战队；关闭则用"数值直传"
        </span>
      </div>

      <el-divider style="margin:8px 0;" />

      <div style="display:flex; gap:18px; flex-wrap:wrap;">
        <!-- 左列：学习力 + 性格 -->
        <div style="flex:1; min-width:260px;">
          <b style="font-size:13px;">学习力</b>
          <span style="margin-left:8px; font-size:12px;"
                :style="{ color: evOver ? 'var(--el-color-danger)' : '#a0a6ad' }">
            总 {{ evTotal }} / {{ EV_TOTAL_CAP }}（单项 ≤255）
          </span>
          <div style="display:grid; grid-template-columns:repeat(3, 1fr); gap:6px; margin-top:6px;">
            <div v-for="(lab, i) in STAT_LABELS" :key="lab">
              <div style="font-size:12px; color:#a0a6ad;">{{ lab }}</div>
              <el-input-number v-model="cul.ev[i]" :min="0" :max="255" size="small"
                               style="width:100%;" :disabled="!cul.enabled" />
            </div>
          </div>
          <div style="margin-top:8px;">
            <b style="font-size:13px;">称号</b>
            <el-select v-model="cul.titleId" filterable size="small"
                       style="width:100%; margin-top:4px;" :disabled="!cul.enabled">
              <el-option :value="0" label="无" />
              <el-option v-for="t in (cultivateMeta.titles || [])" :key="t.id" :value="t.id"
                         :label="`${t.name}｜${t.stats.map((v, i) => v ? ['攻','特攻','防','特防','速','体'][i] + '+' + v : '').filter(Boolean).join('/')}${t.exclusive ? '（专属#' + t.exclusive + '）' : ''}`" />
            </el-select>
          </div>
          <div style="margin-top:8px;">
            <b style="font-size:13px;">性格</b>
            <el-select v-model="cul.natureId" filterable size="small"
                       style="width:100%; margin-top:4px;" :disabled="!cul.enabled">
              <el-option :value="-1" label="无（中性 1.0）" />
              <el-option v-for="n in cultivateMeta.natures" :key="n.id" :value="n.id"
                         :label="`${n.name}｜${n.des || n.mult.map((m) => (m === 1 ? '' : (m > 1 ? '+10%' : '-10%'))).filter(Boolean).join('/')}`" />
            </el-select>
          </div>
          <div style="margin-top:8px;">
            <b style="font-size:13px;">称号</b>
            <span style="margin-left:6px; font-size:12px; color:#a0a6ad;">性格后平加，不吃性格修正</span>
            <el-select v-model="cul.titleId" filterable size="small" clearable
                       placeholder="无称号" style="width:100%; margin-top:4px;"
                       :disabled="!cul.enabled" @clear="cul.titleId = 0">
              <el-option :value="0" label="无称号" />
              <el-option v-for="t in cultivateMeta.titles || []" :key="t.id" :value="t.id"
                         :label="titleLabel(t)">
                <span>{{ t.name }}</span>
                <span style="color:#a0a6ad; font-size:12px; margin-left:6px;">{{ titleStatText(t) }}</span>
                <span v-if="t.targetMonster"
                      style="color:var(--el-color-warning); font-size:12px; margin-left:6px;">
                  专属:#{{ t.targetMonster }}
                </span>
              </el-option>
            </el-select>
          </div>
        </div>

        <!-- 右列：刻印 + 账号级 -->
        <div style="flex:1; min-width:260px;">
          <b style="font-size:13px;">刻印（3 孔）</b>
          <div style="display:flex; flex-direction:column; gap:6px; margin-top:6px;">
            <el-select v-for="(slot, i) in 3" :key="i" v-model="cul.inscriptions[i]"
                       filterable remote clearable size="small"
                       :remote-method="(q) => onKeryinSearch(q, i)" :loading="keryinLoading"
                       placeholder="搜索刻印（名称/效果）" :disabled="!cul.enabled"
                       @visible-change="(v) => v && !keryinOptions[i].length && searchKeryin('', i)">
              <el-option v-for="k in keryinOptions[i]" :key="k.id" :value="k.id" :label="keryinLabel(k)">
                <span>{{ k.name }}</span>
                <span style="color:#a0a6ad; font-size:12px; margin-left:6px;">{{ k.effectDes }}</span>
                <span v-if="k.exclusiveNames?.length"
                      style="color:var(--el-color-warning); font-size:12px; margin-left:6px;">
                  专属:{{ k.exclusiveNames.join('/') }}
                </span>
              </el-option>
            </el-select>
          </div>
          <div style="margin-top:10px; display:flex; flex-direction:column; gap:6px;">
            <el-checkbox v-model="cul.hpCap20" :disabled="!cul.enabled">
              体力上限 +{{ cultivateMeta.hpCapBonus }}
            </el-checkbox>
            <el-checkbox v-model="cul.guild" :disabled="!cul.enabled">
              战队加成（体30 攻/特攻/防/特防15 速10，工作值）
            </el-checkbox>
            <el-checkbox v-model="cul.annual" :disabled="!cul.enabled">
              年费加成（六维+10，不吃性格）
            </el-checkbox>
          </div>
        </div>
      </div>

      <el-divider style="margin:8px 0;" />

      <!-- 预览 -->
      <div style="display:flex; align-items:center; gap:8px;">
        <b style="font-size:13px;">面板预览</b>
        <el-radio-group v-model="activeView" size="small">
          <el-radio-button value="base">基础（双场景）</el-radio-button>
          <el-radio-button value="pvp">PVP</el-radio-button>
          <el-radio-button value="pve">PVE</el-radio-button>
        </el-radio-group>
        <span v-if="previewing" style="color:#a0a6ad; font-size:12px;">计算中…</span>
      </div>
      <el-alert v-if="preview && preview.errors?.length" type="error" :closable="false"
                style="margin-top:6px;"
                :title="preview.errors.join('；')" />
      <table v-if="viewStats" class="cul-preview">
        <thead>
          <tr><th></th><th v-for="s in viewStats" :key="s.label">{{ s.label }}</th></tr>
        </thead>
        <tbody>
          <tr>
            <td style="color:#a0a6ad;">种族</td>
            <td v-for="s in viewStats" :key="'r' + s.label">{{ preview.race[s.label] }}</td>
          </tr>
          <tr>
            <td style="color:#a0a6ad;">合成</td>
            <td v-for="s in viewStats" :key="'v' + s.label">
              <b>{{ s.value }}</b>
              <span style="color:#67c23a; font-size:11px; margin-left:2px;">
                +{{ s.value - preview.race[s.label] }}
              </span>
            </td>
          </tr>
        </tbody>
      </table>
      <div style="color:#a0a6ad; font-size:12px; margin-top:4px;">
        套装/魂印加成在对局内结算（吃上面全部固定值的百分比），不进本预览。
      </div>

      <!-- 高级：抗性训练 -->
      <el-collapse style="margin-top:10px;">
        <el-collapse-item title="抗性训练（高级；不进面板，落精灵抗性字段）">
          <div style="display:flex; align-items:center; gap:10px; margin-bottom:8px;">
            <el-switch v-model="cul.resist.enabled" :disabled="!cul.enabled" />
            <span>启用（未开启的精灵永不触发抗性抵抗；伤害钳 35%、异常槽钳 50%）</span>
          </div>
          <template v-if="cul.resist.enabled">
            <div style="display:flex; gap:6px; align-items:center;">
              <span style="font-size:12px; color:#a0a6ad;">伤害抗性</span>
              <span v-for="(lab, i) in ['暴击', '固定', '百分比']" :key="lab" style="font-size:12px;">
                {{ lab }}<el-input-number v-model="cul.resist.damage[i]" :min="0" :max="35"
                                          size="small" style="width:90px; margin-left:2px;" />
              </span>
            </div>
            <div v-for="(group, gi) in [['control', '控制类'], ['uncontrol', '非控制类']]"
                 :key="group[0]" style="margin-top:8px;">
              <div style="font-size:12px; color:#a0a6ad;">{{ group[1] }}异常抗性槽（≤3）</div>
              <div v-for="(p, i) in cul.resist[group[0]]" :key="i"
                   style="display:flex; gap:6px; margin-top:4px; align-items:center;">
                <el-select v-model="p.anomalyId" filterable size="small" style="width:160px;"
                           placeholder="异常">
                  <el-option v-for="a in meta.anomalies" :key="a.id"
                             :value="a.id" :label="`${a.name}(${a.id})`" />
                </el-select>
                <el-input-number v-model="p.pct" :min="0" :max="50" size="small" style="width:100px;" />
                <el-button size="small" text type="danger"
                           @click="cul.resist[group[0]].splice(i, 1)">删</el-button>
              </div>
              <el-button size="small" text type="primary" style="margin-top:4px;"
                         :disabled="cul.resist[group[0]].length >= 3"
                         @click="cul.resist[group[0]].push({ anomalyId: 0, pct: 50 })">
                + 加一槽
              </el-button>
            </div>
            <div style="margin-top:8px; font-size:12px; color:#a0a6ad;">
              全异常免疫（官方 200 绿宝石解锁，概率待核）
              <el-input-number v-model="cul.resist.completeImmunity" :min="0" :max="100"
                               size="small" style="width:100px; margin-left:6px;" />%
            </div>
          </template>
        </el-collapse-item>
      </el-collapse>
    </template>

    <template #footer>
      <el-button @click="visible = false">取消</el-button>
      <el-button type="primary" @click="confirm">应用</el-button>
    </template>
  </el-dialog>
</template>

<style scoped>
.cul-preview { border-collapse: collapse; margin-top: 6px; font-size: 13px; }
.cul-preview th, .cul-preview td { border: 1px solid var(--el-border-color-lighter); padding: 3px 10px; text-align: center; }
.cul-preview th { color: #a0a6ad; font-weight: normal; }
</style>
