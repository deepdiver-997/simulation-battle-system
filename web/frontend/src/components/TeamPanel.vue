<script setup>
import { ref } from 'vue'
import { ElMessage } from 'element-plus'
import { team, emptySlot, standbySlots } from '../store'
import PetPicker from './PetPicker.vue'
import CultivateEditor from './CultivateEditor.vue'

const props = defineProps({
  side: { type: String, required: true },   // 'side1' | 'side2'
  title: { type: String, required: true },
  pack: { type: String, default: 'battle' }, // battle=出战背包 | standby=待命背包
})

const slots = props.pack === 'standby'
  ? standbySlots[props.side]
  : team[props.side]
const editing = ref(-1)
const pickerVisible = ref(false)
const culEditing = ref(-1)
const culVisible = ref(false)

function openSlot(i) {
  editing.value = i
  pickerVisible.value = true
}

function openCultivate(i) {
  culEditing.value = i
  culVisible.value = true
}

const culActive = (slot) => !!slot.cultivate?.enabled

const skillName = (slot, sid) =>
  slot.detail?.learnableMoves?.find((m) => m.id === sid)?.name || `#${sid}`
const isFifth = (slot, i) =>
  i === 4 && (slot.detail?.fifthCandidates || []).some((m) => m.id === slot.skills[4])
const hasCustomBase = (slot) => (slot.base || []).some((v) => v > 0)

function onConfirm(slotData) {
  slots[editing.value] = slotData
  pickerVisible.value = false
}

function removeSlot(i) {
  slots[i] = emptySlot()
  if (editing.value === i) editing.value = -1
}

// ── 覆盖：先点源槽"覆盖"进入待粘贴状态，再点目标槽位即整份搬过去 ──
const copyArmed = ref(-1)   // -1 = 未进入；>=0 = 已复制的源槽下标
function armCopy(i) {
  copyArmed.value = copyArmed.value === i ? -1 : i
  if (copyArmed.value >= 0) ElMessage.info(`已复制 ${i + 1} 号位，点击目标槽位覆盖（再点源槽取消）`)
}
function deepCopy(slot) {
  return {
    ...slot,
    skills: [...(slot.skills || [])],
    base: [...(slot.base || [0, 0, 0, 0, 0, 0])],
    oracle: !!slot.oracle,
    effectParams: (slot.effectParams || []).map((e) => ({ ...e })),
    disabledEffects: {
      skillEffects: [...(slot.disabledEffects?.skillEffects || [])],
      soulMarks: [...(slot.disabledEffects?.soulMarks || [])],
    },
    // 培养（培养线）：整对象深拷贝，含 ev/inscriptions 数组与 resist 槽
    cultivate: slot.cultivate
      ? {
          ...slot.cultivate,
          ev: [...(slot.cultivate.ev || [0, 0, 0, 0, 0, 0])],
          inscriptions: [...(slot.cultivate.inscriptions || [0, 0, 0])],
          resist: {
            ...slot.cultivate.resist,
            damage: [...(slot.cultivate.resist?.damage || [0, 0, 0])],
            control: (slot.cultivate.resist?.control || []).map((p) => ({ ...p })),
            uncontrol: (slot.cultivate.resist?.uncontrol || []).map((p) => ({ ...p })),
          },
        }
      : undefined,
  }
}
// 槽位统一入口：覆盖模式下点击 = 粘贴；否则 = 打开配置
function onCardClick(i) {
  if (copyArmed.value >= 0) {
    const from = copyArmed.value
    if (from === i) { copyArmed.value = -1; return }
    slots[i] = deepCopy(slots[from])
    copyArmed.value = -1
    ElMessage.success(`已覆盖到 ${i + 1} 号位`)
    return
  }
  openSlot(i)
}

// ── 拖动交换：把一个槽拖到另一个槽上，两槽配置整体对调（含空槽=移动）──
const dragFrom = ref(-1)
const dragOver = ref(-1)
function onDragStart(i, e) {
  if (copyArmed.value >= 0) { e.preventDefault(); return }  // 覆盖模式优先
  dragFrom.value = i
  e.dataTransfer.effectAllowed = 'move'
  e.dataTransfer.setData('text/plain', String(i))  // Firefox 需要非空 data 才起拖
}
function onDragOver(i, e) {
  if (dragFrom.value < 0) return
  e.preventDefault()
  dragOver.value = i
}
function onDrop(i, e) {
  e.preventDefault()
  const from = dragFrom.value
  dragFrom.value = -1
  dragOver.value = -1
  if (from < 0 || from === i) return
  const tmp = slots[from]
  slots[from] = slots[i]
  slots[i] = tmp
}
</script>

<template>
  <div>
    <div style="display:flex; align-items:center; margin-bottom:6px;">
      <b style="font-size:15px;">{{ title }}</b>
      <slot name="host-badge" />
      <el-button size="small" text type="danger" style="margin-left:auto"
                 @click="slots.forEach((s, i) => (slots[i] = emptySlot()))">
        清空
      </el-button>
    </div>
    <el-alert v-if="copyArmed >= 0" type="warning" :closable="false" style="margin-bottom:6px;"
              :title="`覆盖模式：点击目标槽位粘贴 ${copyArmed + 1} 号位配置`" />
    <el-card v-for="(slot, i) in slots" :key="i" class="slot-card" shadow="never"
             :body-style="{ padding: '8px 12px' }"
             :class="{ 'drag-over': dragOver === i, 'dragging': dragFrom === i, 'copy-target': copyArmed >= 0 && copyArmed !== i }"
             :draggable="slot.petId > 0"
             @dragstart="onDragStart(i, $event)" @dragover="onDragOver(i, $event)"
             @dragleave="dragOver === i && (dragOver = -1)" @drop="onDrop(i, $event)"
             @dragend="dragFrom = -1; dragOver = -1">
      <div v-if="slot.petId">
        <div style="display:flex; align-items:center; gap:6px; cursor:pointer;" @click="onCardClick(i)">
          <span class="drag-handle" title="拖动交换位置">⋮⋮</span>
          <b>{{ i + 1 }}. {{ slot.name }}</b>
          <el-tag size="small" type="info">#{{ slot.petId }}</el-tag>
          <el-tag v-if="slot.detail?.has4399" size="small" type="success">4399</el-tag>
          <el-tag v-if="slot.oracle" size="small" type="warning">神谕</el-tag>
          <el-tag v-if="slot.traitId" size="small" type="warning">特性 {{ slot.traitId }}</el-tag>
          <el-tag v-if="culActive(slot)" size="small" type="success">培养</el-tag>
          <el-tag v-if="slot.effectParams && slot.effectParams.length" size="small" type="danger" effect="plain">
            词条已改 ×{{ slot.effectParams.length }}
          </el-tag>
          <span style="margin-left:auto; display:flex; gap:2px;" @click.stop>
            <el-button size="small" text type="success" @click="openCultivate(i)">培养</el-button>
            <el-button size="small" text type="primary" @click="armCopy(i)">覆盖</el-button>
            <el-button size="small" text type="danger" @click="removeSlot(i)">删除</el-button>
          </span>
        </div>
        <div style="margin-top:4px; display:flex; flex-wrap:wrap; gap:4px; cursor:pointer;" @click="onCardClick(i)">
          <el-tag v-for="(sid, i) in slot.skills" :key="sid" size="small"
                  :type="isFifth(slot, i) ? 'warning' : 'info'" effect="plain">
            {{ skillName(slot, sid) }}{{ isFifth(slot, i) ? ' (第五)' : '' }}
          </el-tag>
          <el-tag v-if="hasCustomBase(slot)" size="small" type="danger" effect="plain">
            数值已改
          </el-tag>
        </div>
      </div>
      <el-empty v-else description="" :image-size="24" style="cursor:pointer; padding:2px;"
                @click="onCardClick(i)">
        <template #description>
          <span style="color:#a0a6ad;">{{ i + 1 }}. 点击配置精灵</span>
        </template>
      </el-empty>
    </el-card>

    <PetPicker v-model="pickerVisible" :slot-data="slots[Math.max(editing, 0)] || {}"
               @confirm="onConfirm" />
    <CultivateEditor v-model="culVisible"
                     :slot-data="slots[Math.max(culEditing, 0)] || {}" />
  </div>
</template>

<style scoped>
.slot-card { margin-bottom: 6px; transition: box-shadow .15s, border-color .15s; }
.slot-card[draggable="true"] { cursor: grab; }
.slot-card.dragging { opacity: .45; }
.slot-card.drag-over { border: 1px dashed var(--el-color-primary); }
.slot-card.copy-target { border: 1px dashed var(--el-color-warning); }
.drag-handle { color: #b0b6bf; cursor: grab; font-weight: bold; letter-spacing: -2px; }
</style>
