<script setup>
import { ref, onMounted } from 'vue'
import { ElMessage } from 'element-plus'
import { get } from '../api'
import { team, battle, meta, startBattle, stopBattle, loadMeta, lineupPayload,
         standbySlots, bpOrder, bpBanned } from '../store'
import TeamPanel from './TeamPanel.vue'
import BattlePanel from './BattlePanel.vue'
import TapePanel from './TapePanel.vue'

const starting = ref(false)

onMounted(loadMeta)

// ---- BP 出战选序（双背包模型：出战 6 + 待命 6 = 12 只资产池，选序构造 elfPets）----
// bpBanned/bpOrder 都在 store（sideLineup 构造名单时要吃禁选排除）
const orderDialog = ref(null)   // { side } | null
const banned = ref([])          // [{pack, idx}] 对手禁的（≤3）
const picked = ref([])          // [{pack, idx}] 出场顺序（1~6）

function openOrder(side) {
  orderDialog.value = { side }
  banned.value = [...(bpBanned[side] || [])]
  picked.value = [...(bpOrder[side] || [])]
}
function orderCards(side) {
  const cards = []
  team[side].forEach((s, idx) => { if (s.petId > 0) cards.push({ pack: 'battle', idx, name: s.name, petId: s.petId }) })
  standbySlots[side].forEach((s, idx) => { if (s.petId > 0) cards.push({ pack: 'standby', idx, name: s.name, petId: s.petId }) })
  return cards
}
const isBanned = (c) => banned.value.some((b) => b.pack === c.pack && b.idx === c.idx)
const isPicked = (c) => picked.value.some((p) => p.pack === c.pack && p.idx === c.idx)
function toggleBan(c) {
  const i = banned.value.findIndex((b) => b.pack === c.pack && b.idx === c.idx)
  if (i >= 0) banned.value.splice(i, 1)
  else {
    if (banned.value.length >= 3) { ElMessage.warning('最多禁 3 只'); return }
    banned.value.push({ pack: c.pack, idx: c.idx })
    const pi = picked.value.findIndex((p) => p.pack === c.pack && p.idx === c.idx)
    if (pi >= 0) picked.value.splice(pi, 1)
  }
}
function pickIn(c) {
  if (isBanned(c) || isPicked(c)) return
  if (picked.value.length >= 6) { ElMessage.warning('出场最多 6 只'); return }
  picked.value.push({ pack: c.pack, idx: c.idx })
}
function pickOut(c) {
  const i = picked.value.findIndex((p) => p.pack === c.pack && p.idx === c.idx)
  if (i >= 0) picked.value.splice(i, 1)
}
function saveOrder() {
  const side = orderDialog.value.side
  bpOrder[side] = picked.value.map((p) => ({ ...p }))
  bpBanned[side] = banned.value.map((b) => ({ ...b }))
  orderDialog.value = null
  ElMessage.success(`出场选序已保存（${picked.value.length} 只）`)
}

// ---- 药剂库存 ----
function addMedicine(side) {
  const pick = medPick.value[side]
  if (!pick) return
  const cur = team.medicines[side][pick] || 0
  team.medicines[side][pick] = cur + 5
}
function removeMedicine(side, itemId) { delete team.medicines[side][itemId] }
const medPick = ref({ side1: '', side2: '' })
const equipPick = ref({ side1: '', side2: '' })
// 套装一键穿戴（2026-09-29）：选套装 → 全部部件入清单（成套激活由引擎按件数判定）
const suitPick = ref({ side1: '', side2: '' })
const suitOptions = ref({ side1: [], side2: [] })
async function searchSuit(q, side) {
  try {
    const data = await get('/cultivation/suits', { q, limit: 30 })
    suitOptions.value[side] = data.suits || []
  } catch { /* 网关未起 */ }
}
function wearSuit(side) {
  const suit = suitOptions.value[side].find((x) => String(x.id) === String(suitPick.value[side]))
  if (!suit) return
  const cur = new Set(team.equip[side])
  for (const pc of suit.pieces) cur.add(pc.id)
  team.equip[side] = [...cur]
  suitPick.value[side] = ''
}

// ---- 装备/套装（2026-09-28 三视角线）：队伍级穿戴清单，成套数值引擎落本体 ----
const equipOptions = ref({ side1: [], side2: [] })
async function searchEquip(q, side) {
  try {
    const data = await get('/cultivation/equips', { q, limit: 30 })
    equipOptions.value[side] = data.equips || []
  } catch { /* 网关未起 */ }
}
const equipName = (itemId) => {
  for (const side of ['side1', 'side2']) {
    const hit = equipOptions.value[side].find((e) => String(e.id) === String(itemId))
    if (hit) return hit.name
  }
  return '#' + itemId
}
const medName = (itemId) => meta.medicines.find((m) => String(m.itemId) === String(itemId))?.name || ('#' + itemId)

async function useSample() {
  const sample = await get('/sample-lineup')
  const fill = async (side, pet) => {
    const detail = await get(`/pets/${pet.petId}`)
    for (let i = 0; i < 6; ++i) {
      team[side][i] = {
        petId: pet.petId, name: detail.name, detail,
        skills: [...pet.skills], traitId: pet.traitId || 0,
        base: [...(pet.base || [0, 0, 0, 0, 0, 0])],
      }
    }
  }
  await fill('side1', Array.isArray(sample.side1) ? sample.side1[0] : sample.side1)
  await fill('side2', Array.isArray(sample.side2) ? sample.side2[0] : sample.side2)
}

async function begin() {
  let lineup
  try {
    lineup = lineupPayload()
  } catch (e) {
    ElMessage.warning(e.message)
    return
  }
  // BP 禁选是客户端强制（引擎协议没有 ban 概念）：被禁卡在构造名单时被排除，这里提示一句
  if (team.mode === 'bp') {
    const packs = (side) => ({ battle: team[side], standby: standbySlots[side] })
    const excluded = ['side1', 'side2'].flatMap((side) =>
      (bpBanned[side] || []).map(({ pack, idx }) => packs(side)[pack][idx])
        .filter((s) => s && s.petId > 0))
    if (excluded.length) {
      ElMessage.info(`BP 禁选生效：${excluded.map((s) => s.name).join('、')} 不进出场名单`)
    }
  }
  starting.value = true
  try {
    startBattle(lineup)
  } finally {
    starting.value = false
  }
}
</script>

<template>
  <el-row :gutter="14">
    <el-col :span="6">
      <TeamPanel side="side1" title="🟦 我方 · 出战背包">
        <template #host-badge>
          <el-tag size="small" type="warning" effect="dark" style="margin-left:4px;">房主</el-tag>
        </template>
      </TeamPanel>
      <TeamPanel side="side1" title="🟦 我方 · 待命背包" pack="standby" />
    </el-col>

    <el-col :span="12">
      <el-card shadow="never" style="margin-bottom:10px;">
        <div style="display:flex; gap:8px; align-items:center; flex-wrap:wrap;">
          <el-button type="primary" :disabled="battle.phase === 'running'" :loading="starting"
                     @click="begin">
            {{ battle.phase === 'running' ? '对战中…' : '▶ 开始对战' }}
          </el-button>
          <el-button :disabled="battle.phase !== 'running'" @click="stopBattle">断开</el-button>
          <el-button @click="useSample">示例阵容</el-button>
          <el-divider direction="vertical" />
          <span class="mono" style="color:#909399;">
            {{ battle.snapshot ? `回合 ${battle.snapshot.round} · ${battle.snapshot.stateName || ''}` : '尚未开局' }}
          </span>
        </div>
        <el-divider style="margin:10px 0;" />
        <!-- 组队模式 -->
        <el-form label-width="86px" size="small" style="margin-bottom:0;">
          <el-form-item label="组队模式">
            <el-radio-group v-model="team.mode" :disabled="battle.phase === 'running'">
              <el-radio-button value="direct">直开 6v6</el-radio-button>
              <el-radio-button value="bp">BP 赛前快照</el-radio-button>
            </el-radio-group>
            <el-tooltip content="boss 挑战：魂印中标注 boss无效 的子句不注册（索伦森①②压强化/谱尼轮回虚无元素永恒/混沌魔尊等已标注生效，其余魂印标注陆续补）"
                        placement="top">
              <span style="margin-left:12px;">
                <el-switch v-model="team.bossChallenge" :disabled="battle.phase === 'running'"
                           inline-prompt active-text="boss挑战" inactive-text="PvP" size="small" />
              </span>
            </el-tooltip>
            <span style="margin-left:8px; color:#909399; font-size:12px;">
              {{ team.mode === 'bp'
                ? '选序+禁选生效，传两张背包快照（出战 6 + 待命 6）；格劳瑞自爆等”赛前读者”按双背包语义结算'
                : '只认出战背包按序（选序/禁选/待命均不参与）；待命 6 格全空（格劳瑞类读者降级并记偏差）' }}
            </span>
          </el-form-item>

          <el-form-item label="出战选序">
            <el-button size="small" @click="openOrder('side1')">我方选序</el-button>
            <el-button size="small" @click="openOrder('side2')">对方选序</el-button>
            <span style="color:#909399; font-size:12px; margin-left:6px;">
              从 12 只资产池挑出场名单（先标对手禁的 ≤3 只，再按序选 1~6 只）；不选 = 出战背包按序
            </span>
          </el-form-item>

          <el-form-item label="装备套装">
            <div style="width:100%;">
              <div v-for="side in ['side1','side2']" :key="side" style="display:flex; gap:6px; align-items:center; margin-bottom:4px; flex-wrap:wrap;">
                <span class="mono" style="color:#909399; width:36px;">{{ side === 'side1' ? '我方' : '对方' }}</span>
                <el-tag v-for="itemId in team.equip[side]" :key="itemId" closable size="small"
                        type="warning" effect="plain" @close="team.equip[side] = team.equip[side].filter((x) => x !== itemId)">
                  {{ equipName(itemId) }}
                </el-tag>
                <el-select v-model="equipPick[side]" filterable remote clearable placeholder="搜索装备（名称/效果）"
                           size="small" style="width:240px;"
                           :remote-method="(q) => searchEquip(q, side)"
                           @visible-change="(v) => v && !equipOptions[side].length && searchEquip('', side)"
                           @change="(id) => { if (id && !team.equip[side].includes(id)) team.equip[side].push(id); equipPick[side] = '' }">
                  <el-option v-for="e in equipOptions[side]" :key="e.id" :value="e.id"
                             :label="`${e.name}｜${e.desc.slice(0, 24)}`" />
                </el-select>
              </div>
              <div v-for="side in ['side1','side2']" :key="side + '-suit'" style="display:flex; gap:6px; align-items:center; margin-bottom:4px; flex-wrap:wrap;">
                <span class="mono" style="color:#909399; width:36px;">{{ side === 'side1' ? '我方' : '对方' }}</span>
                <el-select v-model="suitPick[side]" filterable remote clearable placeholder="搜套装 → 一键穿戴整套"
                           size="small" style="width:240px;"
                           :remote-method="(q) => searchSuit(q, side)"
                           @visible-change="(v) => v && !suitOptions[side].length && searchSuit('', side)"
                           @change="() => wearSuit(side)">
                  <el-option v-for="x in suitOptions[side]" :key="x.id" :value="String(x.id)"
                             :label="`${x.name}（${x.pieces.length}件）`">
                    <span>{{ x.name }}（{{ x.pieces.length }}件）</span>
                    <span style="color:#a0a6ad; font-size:12px; margin-left:6px;">{{ x.pieces[0]?.desc }}</span>
                  </el-option>
                </el-select>
              </div>
              <div style="color:#a0a6ad; font-size:12px;">成套数值由引擎按对局维度结算；预览面板可切基础/PVP/PVE 三视角</div>
            </div>
          </el-form-item>
          <el-form-item label="药剂库存">
            <div style="width:100%;">
              <div v-for="side in ['side1','side2']" :key="side" style="display:flex; gap:6px; align-items:center; margin-bottom:4px; flex-wrap:wrap;">
                <span class="mono" style="color:#909399; width:36px;">{{ side === 'side1' ? '我方' : '对方' }}</span>
                <el-tag v-for="(count, itemId) in team.medicines[side]" :key="itemId" closable size="small"
                        type="success" effect="plain" @close="removeMedicine(side, itemId)">
                  {{ medName(itemId) }} ×{{ count }}
                </el-tag>
                <el-select v-model="medPick[side]" filterable placeholder="选药剂" size="small"
                           style="width:180px;" clearable>
                  <el-option v-for="m in meta.medicines" :key="m.itemId" :value="String(m.itemId)"
                             :label="`${m.name}${m.hp ? ' +' + m.hp : ''}${m.pp ? ' PP+' + m.pp : ''}${m.cureAll ? ' 解异常' : ''}`" />
                </el-select>
                <el-button size="small" :disabled="!medPick[side]" @click="addMedicine(side)">+5</el-button>
              </div>
              <div style="color:#909399; font-size:12px;">不配 = 双方无药剂可嗑（合法动作集药剂区显示“无”）</div>
            </div>
          </el-form-item>
        </el-form>
        <el-divider style="margin:10px 0 0;" />
        <el-alert v-if="battle.wsError" :title="battle.wsError" type="error" :closable="false"
                  style="margin-top:8px;" />
        <el-alert v-if="battle.over" :closable="false" style="margin-top:8px;"
                  :type="battle.over.winner === 0 ? 'success' : battle.over.winner === 1 ? 'warning' : 'info'"
                  :title="`战斗结束：${battle.over.winner < 0 ? '平局' : (battle.over.winner === 0 ? '我方胜' : '对方胜')}（回合 ${battle.over.round}，原因 ${battle.over.reason}）`" />
      </el-card>

      <BattlePanel />

      <TapePanel />
    </el-col>

    <el-col :span="6">
      <TeamPanel side="side2" title="🟥 对方 · 出战背包" pack="battle" />
      <TeamPanel side="side2" title="🟥 对方 · 待命背包" pack="standby" />
    </el-col>

    <!-- BP 出战选序 -->
    <el-dialog :model-value="!!orderDialog"
               :title="`出战选序 · ${orderDialog?.side === 'side1' ? '我方' : '对方'}`" width="760px"
               @update:model-value="(v) => (!v) && (orderDialog = null)">
      <div v-if="orderDialog" style="display:flex; gap:12px;">
        <div style="flex:1.4;">
          <div style="color:#909399; font-size:12px; margin-bottom:6px;">
            资产池（点卡片标禁 ≤3；再点"选入"按序进出场名单）
          </div>
          <div style="display:flex; flex-wrap:wrap; gap:6px;">
            <div v-for="c in orderCards(orderDialog.side)" :key="c.pack + c.idx"
                 class="bp-card"
                 :class="{ 'bp-banned': isBanned(c), 'bp-picked': isPicked(c) }">
              <span style="font-weight:bold;">{{ c.name }}</span>
              <span class="mono" style="color:#909399; font-size:11px;">
                {{ c.pack === 'battle' ? '战' : '待' }}#{{ c.petId }}
              </span>
              <span style="margin-left:auto; display:flex; gap:4px;">
                <el-button size="small" text type="danger"
                           @click="toggleBan(c)">{{ isBanned(c) ? '已禁' : '禁' }}</el-button>
                <el-button size="small" text type="primary" :disabled="isBanned(c) || isPicked(c)"
                           @click="pickIn(c)">选入</el-button>
              </span>
            </div>
          </div>
        </div>
        <div style="flex:1;">
          <div style="color:#909399; font-size:12px; margin-bottom:6px;">
            出场顺序（{{ picked.length }} 只；1 号位首发，其余按序替补）
          </div>
          <div v-for="(p, i) in picked" :key="p.pack + p.idx" class="bp-picked-row">
            <span class="mono">{{ i + 1 }}.</span>
            <span style="font-weight:bold; margin-left:4px;">
              {{ (p.pack === 'battle' ? team[orderDialog.side] : standbySlots[orderDialog.side])[p.idx].name }}
            </span>
            <span class="mono" style="color:#909399; font-size:11px; margin-left:4px;">
              {{ p.pack === 'battle' ? '战' : '待' }}
            </span>
            <el-button size="small" text type="danger" style="margin-left:auto;"
                       @click="pickOut(p)">移除</el-button>
          </div>
          <div v-if="!picked.length" style="color:#a8abb2; font-size:12px;">尚未选入</div>
        </div>
      </div>
      <template #footer>
        <el-button @click="orderDialog = null">取消</el-button>
        <el-button type="primary" @click="saveOrder">保存选序</el-button>
      </template>
    </el-dialog>

  </el-row>
</template>

<style scoped>
.bp-card {
  width: 210px; display: flex; align-items: center; gap: 6px;
  border: 1px solid #dcdfe6; border-radius: 6px; padding: 6px 8px;
  font-size: 13px; cursor: pointer; margin-bottom: 6px;
}
.bp-card.bp-banned { opacity: .45; text-decoration: line-through; }
.bp-card.bp-picked { border-color: var(--el-color-primary); background: #ecf5ff; }
.bp-picked-row {
  display: flex; align-items: center; gap: 4px;
  border: 1px solid #e4e7ed; border-radius: 6px; padding: 4px 8px; margin-bottom: 4px;
}
</style>
