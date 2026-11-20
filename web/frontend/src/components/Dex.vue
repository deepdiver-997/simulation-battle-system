<script setup>
import { ref, watch } from 'vue'
import { get } from '../api'

const kind = ref('pet')
const q = ref('')
const pets = ref([])
const moves = ref([])
const traits = ref([])
const petDetail = ref(null)
const drawer = ref(false)

const statusClass = (s) => `badge-${s}`

watch([q, kind], search)
let seq = 0
async function search() {
  const my = ++seq
  const query = { q: q.value, limit: 80 }
  if (kind.value === 'pet') {
    const d = await get('/pets', query)
    if (my === seq) { pets.value = d.pets; moves.value = []; traits.value = [] }
  } else if (kind.value === 'move') {
    const d = await get('/moves', query)
    if (my === seq) { moves.value = d.moves; pets.value = []; traits.value = [] }
  } else {
    const d = await get('/traits', { q: q.value, limit: 80 })
    if (my === seq) { traits.value = d.traits; pets.value = []; moves.value = [] }
  }
}

async function openPet(row) {
  petDetail.value = await get(`/pets/${row.petId}`)
  drawer.value = true
}
</script>

<template>
  <div>
    <div style="display:flex; gap:12px; align-items:center; margin-bottom:10px;">
      <el-radio-group v-model="kind">
        <el-radio-button value="pet">精灵</el-radio-button>
        <el-radio-button value="move">技能</el-radio-button>
        <el-radio-button value="trait">通用特性</el-radio-button>
      </el-radio-group>
      <el-input v-model="q" style="max-width:420px;" clearable
                placeholder="输入名字子串或编号，如：涂蝶 / 4861 / 寒蝉" />
      <span style="color:#909399; font-size:13px;">
        展示口径：4399 图鉴文本优先，数据库仅作回退（神谕等 4399 没有的内容）
      </span>
    </div>

    <!-- 精灵结果 -->
    <el-table v-if="kind === 'pet'" :data="pets" size="small" @row-click="openPet" highlight-current-row>
      <el-table-column prop="petId" label="图鉴号" width="90" />
      <el-table-column prop="name" label="精灵" min-width="140" />
      <el-table-column prop="element" label="属性" width="140" />
      <el-table-column label="来源" width="100">
        <template #default="{ row }">
          <el-tag size="small" :type="row.has4399 ? 'success' : 'info'">{{ row.has4399 ? '4399 图鉴' : '仅数据库' }}</el-tag>
        </template>
      </el-table-column>
      <el-table-column label="可组队" width="90">
        <template #default="{ row }">
          <el-tag size="small" :type="row.teamable ? 'success' : 'danger'">{{ row.teamable ? '可' : '否' }}</el-tag>
        </template>
      </el-table-column>
    </el-table>

    <!-- 技能结果 -->
    <el-table v-else-if="kind === 'move'" :data="moves" size="small">
      <el-table-column prop="id" label="ID" width="90" />
      <el-table-column prop="name" label="技能" width="130" />
      <el-table-column prop="element" label="属性" width="80" />
      <el-table-column prop="category" label="类别" width="70" />
      <el-table-column prop="power" label="威力" width="60" />
      <el-table-column prop="pp" label="PP" width="50" />
      <el-table-column label="效果实现" width="110">
        <template #default="{ row }">
          <span :class="statusClass(row.status)" class="badge-dot" />{{ row.badgeLabel }}
        </template>
      </el-table-column>
      <el-table-column prop="info" label="说明" min-width="260" show-overflow-tooltip />
    </el-table>

    <!-- 特性结果 -->
    <el-table v-else :data="traits" size="small">
      <el-table-column prop="idx" label="编号" width="90" />
      <el-table-column prop="name" label="特性" width="140" />
      <el-table-column prop="intro" label="说明" min-width="300" />
      <el-table-column label="实现" width="110">
        <template #default="{ row }">
          <span :class="statusClass(row.status)" class="badge-dot" />{{ row.statusLabel }}
        </template>
      </el-table-column>
    </el-table>

    <!-- 精灵详情抽屉 -->
    <el-drawer v-model="drawer" size="56%" :title="petDetail ? `${petDetail.name} #${petDetail.petId}` : ''">
      <template v-if="petDetail">
        <div class="section-title">基本信息</div>
        <el-descriptions :column="3" border size="small">
          <el-descriptions-item label="属性">{{ petDetail.element || '—' }}</el-descriptions-item>
          <el-descriptions-item label="性别">{{ petDetail.scrape?.gender || petDetail.db?.gender || '—' }}</el-descriptions-item>
          <el-descriptions-item label="稀有度">{{ petDetail.scrape?.level || '—' }}</el-descriptions-item>
          <el-descriptions-item label="类型">{{ petDetail.scrape?.pet_type || '—' }}</el-descriptions-item>
          <el-descriptions-item label="可组队">{{ petDetail.teamable ? '是（可学技能≥5）' : '否' }}</el-descriptions-item>
          <el-descriptions-item label="4399 图鉴">
            <a v-if="petDetail.scrape?.detail_url" :href="petDetail.scrape.detail_url" target="_blank">打开</a>
            <span v-else>无</span>
          </el-descriptions-item>
        </el-descriptions>

        <div class="section-title">种族值</div>
        <el-table :data="[petDetail.scrape?.race || petDetail.db?.stats || {}]" size="small">
          <el-table-column v-for="k in Object.keys(petDetail.scrape?.race || petDetail.db?.stats || {})"
                           :key="k" :prop="k" :label="k" />
        </el-table>

        <div class="section-title">
          魂印
          <el-tag size="small" :type="petDetail.soulSeal4399?.source === '4399' ? 'success' : 'info'">
            {{ petDetail.soulSeal4399?.source === '4399' ? '4399 图鉴' : '数据库回退' }}
          </el-tag>
        </div>
        <p style="white-space:pre-wrap; margin:4px 0;">{{ petDetail.soulSeal4399?.text || '（无魂印数据）' }}</p>
        <el-table v-if="petDetail.soulMarks?.length" :data="petDetail.soulMarks" size="small" style="margin-top:4px;">
          <el-table-column prop="effectId" label="effect" width="80" />
          <el-table-column prop="kind" label="kind" width="80" />
          <el-table-column label="实现" width="110">
            <template #default="{ row }">
              <span :class="statusClass(row.status)" class="badge-dot" />{{ row.statusLabel }}
            </template>
          </el-table-column>
          <el-table-column prop="tips" label="数据库 tips" min-width="260" show-overflow-tooltip />
        </el-table>

        <template v-if="petDetail.scrape?.training">
          <div class="section-title">推荐培养（4399）</div>
          <p style="white-space:pre-wrap; margin:4px 0;">
            {{ petDetail.scrape.training.effort || '' }}
            {{ petDetail.scrape.training.moves || '' }}
          </p>
        </template>

        <template v-if="petDetail.scrape?.skills?.length">
          <div class="section-title">技能表（4399 图鉴）</div>
          <el-table :data="petDetail.scrape.skills" size="small">
            <el-table-column prop="name" label="技能" width="120" />
            <el-table-column prop="category" label="类别" width="100" />
            <el-table-column prop="power" label="威力" width="60" />
            <el-table-column prop="pp" label="PP" width="52" />
            <el-table-column prop="learn_level" label="习得" width="60" />
            <el-table-column prop="effect" label="效果" min-width="300" show-overflow-tooltip />
          </el-table>
        </template>

        <div v-if="petDetail.fifthCandidates?.length" class="section-title">第五技能（hide_moves）</div>
        <el-table v-if="petDetail.fifthCandidates?.length" :data="petDetail.fifthCandidates" size="small">
          <el-table-column prop="id" label="ID" width="84" />
          <el-table-column prop="name" label="技能" width="150" />
          <el-table-column prop="element" label="属性" width="64" />
          <el-table-column prop="power" label="威力" width="60" />
          <el-table-column prop="pp" label="PP" width="52" />
          <el-table-column label="效果实现" width="110">
            <template #default="{ row }">
              <span :class="statusClass(row.badge.status)" class="badge-dot" />{{ row.badgeLabel }}
            </template>
          </el-table-column>
          <el-table-column prop="info" label="说明" min-width="240" show-overflow-tooltip />
        </el-table>

        <div class="section-title">可学技能（数据库 · 战斗以库为准）</div>
        <el-table :data="petDetail.learnableMoves" size="small">
          <el-table-column prop="id" label="ID" width="84" />
          <el-table-column prop="name" label="技能" width="120">
            <template #default="{ row }">
              {{ row.name }}
              <el-tag v-if="(petDetail.fifthCandidates || []).some((m) => m.id === row.id)"
                      size="small" type="warning" effect="plain">第五</el-tag>
            </template>
          </el-table-column>
          <el-table-column prop="element" label="属性" width="64" />
          <el-table-column prop="category" label="类别" width="64" />
          <el-table-column prop="power" label="威力" width="60" />
          <el-table-column prop="pp" label="PP" width="52" />
          <el-table-column label="效果实现" width="110">
            <template #default="{ row }">
              <span :class="statusClass(row.badge.status)" class="badge-dot" />{{ row.badgeLabel }}
            </template>
          </el-table-column>
          <el-table-column prop="info" label="说明" min-width="240" show-overflow-tooltip />
        </el-table>
      </template>
    </el-drawer>
  </div>
</template>
