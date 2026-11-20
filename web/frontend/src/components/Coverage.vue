<script setup>
import { ref, onMounted, computed } from 'vue'
import { get, post } from '../api'

const cov = ref(null)
const filter = ref('')
const refreshing = ref(false)

async function load() {
  cov.value = await get('/coverage')
}
onMounted(load)

async function refresh() {
  refreshing.value = true
  try {
    await post('/coverage/refresh')
    await load()
  } finally {
    refreshing.value = false
  }
}

const rows = computed(() => {
  if (!cov.value) return []
  const out = []
  for (const key of ['moves', 'soul', 'suit']) {
    for (const s of cov.value[key].samples || []) {
      out.push({ group: cov.value[key].label, ...s })
    }
  }
  const f = filter.value.trim()
  if (!f) return out
  return out.filter((r) =>
    String(r.effectId).includes(f) || (r.info || '').includes(f) ||
    (r.moves || []).some((m) => m.includes(f)))
})

const cards = computed(() => cov.value ? ['moves', 'soul', 'suit'].map((k) => cov.value[k]) : [])
</script>

<template>
  <div>
    <el-row :gutter="12" style="margin-bottom:12px;">
      <el-col v-for="c in cards" :key="c.label" :span="8">
        <el-card shadow="never">
          <b>{{ c.label }}</b>
          <div style="margin:8px 0;">库里引用 {{ c.referenced }} 个效果 · 动态库已实现 {{ c.registered }} 个</div>
          <el-progress :percentage="c.referenced ? Math.round(c.implemented * 100 / c.referenced) : 0"
                       :stroke-width="14" :color="c.implemented * 2 >= c.referenced ? '#67c23a' : '#e6a23c'" />
          <div style="margin-top:6px; color:#909399;">
            已实现 {{ c.implemented }} · 未实现 <b style="color:#f56c6c;">{{ c.unimplemented }}</b>
            <template v-if="c.ignored"> · 官方忽略 {{ c.ignored }}</template>
          </div>
        </el-card>
      </el-col>
    </el-row>

    <div style="display:flex; gap:10px; margin-bottom:8px;">
      <el-input v-model="filter" placeholder="过滤：effect id / 描述 / 引用技能名" style="max-width:360px;" clearable />
      <el-button :loading="refreshing" @click="refresh">重新对账（重跑注册表导出）</el-button>
      <span style="color:#909399; align-self:center;">徽标口径：数据库引用的 effect id 对比插件注册表；custom_effect_programs 算已实现，overrides 标注的官方死列不算缺失</span>
    </div>

    <el-table :data="rows" size="small" height="560">
      <el-table-column prop="group" label="类别" width="90" />
      <el-table-column prop="effectId" label="effect id" width="100" />
      <el-table-column prop="info" label="效果描述（库）" min-width="240" show-overflow-tooltip />
      <el-table-column label="引用它的技能" min-width="200">
        <template #default="{ row }">
          <el-tag v-for="m in row.moves" :key="m" size="small" effect="plain" style="margin-right:4px;">{{ m }}</el-tag>
          <span v-if="!row.moves?.length" style="color:#c0c4cc;">—</span>
        </template>
      </el-table-column>
    </el-table>
  </div>
</template>
