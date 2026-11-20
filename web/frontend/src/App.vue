<script setup>
import { ref, onMounted, onUnmounted } from 'vue'
import { get, getToken, setToken } from './api'
import { engineVersionAlert, loadMeta } from './store'
import { feedbackPrompt, openFeedback, closeFeedback, dismissFeedbackForever } from './feedbackPrompt'
import TeamBattle from './components/TeamBattle.vue'
import Dex from './components/Dex.vue'
import Coverage from './components/Coverage.vue'

const GH_ISSUES = 'https://github.com/deepdiver-997/simulation-battle-system/issues'
// 走反馈渠道 = 已触达，永久安静（feedbackPrompt.js 状态机置 done）
function goGithub() {
  dismissFeedbackForever()
  window.open(GH_ISSUES, '_blank', 'noopener')
}
function goMail() {
  dismissFeedbackForever()
  location.href = 'mailto:feedback@scut.email'
}

const tab = ref('battle')
const health = ref(null)

const authDialog = ref(false)
const tokenInput = ref('')
const aboutDialog = ref(false)

function openAuthDialog() {
  tokenInput.value = getToken()
  authDialog.value = true
}
function saveToken() {
  setToken(tokenInput.value)
  authDialog.value = false
  // 可靠起见整页刷新：各页签已失败的首次拉取会重新走一遍
  location.reload()
}

onMounted(async () => {
  window.addEventListener('sbs-unauthorized', openAuthDialog)
  try { health.value = await get('/health') } catch { /* 网关未起 */ }
  loadMeta()   // 指纹入库 + localStorage 跨会话比对（横幅判定源之一）
})
onUnmounted(() => window.removeEventListener('sbs-unauthorized', openAuthDialog))
</script>

<template>
  <el-container>
    <el-header height="auto" style="padding: 0 16px;">
      <div style="display:flex; align-items:center; gap:16px; padding:10px 0 0;">
        <span style="font-size:18px; font-weight:700;">⚔️ sbs 对战调试台</span>
        <el-tabs v-model="tab" style="flex:1;">
          <el-tab-pane label="组队对战" name="battle" />
          <el-tab-pane label="图鉴搜索" name="dex" />
          <el-tab-pane label="效果覆盖率" name="coverage" />
        </el-tabs>
        <el-button text @click="aboutDialog = true" title="项目介绍与使用说明">ℹ️ 说明</el-button>
        <el-button text @click="openFeedback" title="提反馈（issue / 邮件）">💬 反馈</el-button>
        <el-button text @click="openAuthDialog" title="设置访问令牌">🔑 令牌</el-button>
        <!-- 单标签动态样式：el-tag 自带进出场过渡，v-if/v-else 切换在无过渡事件的环境里会残留旧节点 -->
        <el-tag :type="health ? 'success' : 'danger'" effect="plain">
          <template v-if="health">
            sim {{ health.sim.host }}:{{ health.sim.port }} ·
            已实现 moves {{ health.registry.moves }} / soul {{ health.registry.soul }} /
            suit {{ health.registry.suit }}
          </template>
          <template v-else>网关未连接</template>
        </el-tag>
      </div>
    </el-header>
    <!-- 引擎版本报警（HELLO 指纹握手，2026-10-01）：不可关闭，响亮是它存在的意义 -->
    <el-alert v-if="engineVersionAlert.active" type="error" :closable="false" show-icon
              :title="engineVersionAlert.detail"
              style="margin: 0 16px 8px;" />
    <el-main class="page">
      <TeamBattle v-if="tab === 'battle'" />
      <Dex v-else-if="tab === 'dex'" />
      <Coverage v-else />
    </el-main>

    <el-dialog v-model="aboutDialog" title="关于这个对战调试台" width="560px">
      <div style="display:flex; flex-direction:column; gap:10px; line-height:1.7; font-size:13.5px;">
        <div>
          <b>这是什么</b><br />
          非官方粉丝研究项目：以官方口径逐条还原对战判定——先制、盔/威/封属、消逝、
          属性克制与伤害管线，160+ 场景测试锁定口径，与游戏内表现逐条对拍。
        </div>
        <div>
          <b>三步上手</b><br />
          ① 右上角 🔑 输入令牌 <span class="mono">sbs-3021308f</span>（输一次浏览器会记住）→
          ② 点「示例阵容」→ ③ 点「▶ 开始对战」。
          开局后右侧面板可出招/换宠/嗑药，打开「自动」可挂机看完整对局；
          「逐步模式」逐时点推进，配合事件带查看每一步判定；
          队伍槽位的「培养」面板可配学习力/性格/刻印并实时预览数值。
        </div>
        <div>
          <b>🌏 服务器在美国</b><br />
          服务部署在美国服务器，直连不一定成功：加载慢或超时请多重试几次，或换个网络环境。
        </div>
        <div>
          <b>发现和游戏内表现不一致？</b><br />
          欢迎到 <a href="https://github.com/deepdiver-997/simulation-battle-system/issues" target="_blank" rel="noopener">GitHub 提 issue</a>，附调试台「导出本局 JSON」更好定位；
          不方便用 GitHub 也可 <a href="mailto:feedback@scut.email">邮件反馈</a>。
        </div>
        <div style="color:#909399; font-size:12px;">
          打不开/白屏排查：换最新版 Chrome 或 Safari 再试（旧 App 内置 webview 内核过旧）；
          页面底部红字横幅的文本就是病因，反馈时请一并附上。
        </div>
      </div>
      <template #footer>
        <el-button type="primary" @click="aboutDialog = false">开始使用</el-button>
      </template>
    </el-dialog>

    <!-- 反馈提醒（feedbackPrompt.js）：完成对局达阈值自动弹一次；「💬 反馈」按钮手动打开同款。
         右上 X / ESC / 点遮罩关 = 自动模式下按"下次再说"处理（额度触发时已记入 localStorage） -->
    <el-dialog v-model="feedbackPrompt.visible"
               :title="feedbackPrompt.manual ? '💬 反馈' : '觉得这个调试台怎么样？'"
               width="440px" @closed="closeFeedback">
      <div style="line-height:1.8; font-size:13.5px;">
        <p v-if="!feedbackPrompt.manual" style="margin:0 0 8px;">
          已经跑了几局，希望它帮到了你 🙂 如果发现了和游戏内表现不一致的地方，或有功能建议，欢迎告诉我们——几句就行。
        </p>
        <p v-else style="margin:0 0 8px;">
          发现和游戏内表现不一致，或有功能建议？两种渠道都欢迎。
        </p>
        <p style="margin:0 0 14px; color:#909399; font-size:12.5px;">
          反馈判定不一致时，附调试台「导出本局 JSON」更好定位。
        </p>
        <div style="display:flex; gap:10px;">
          <el-button type="primary" @click="goGithub" style="flex:1;">GitHub 提 issue</el-button>
          <el-button @click="goMail" style="flex:1;">邮件反馈</el-button>
        </div>
      </div>
      <template #footer>
        <template v-if="feedbackPrompt.manual">
          <el-button @click="closeFeedback">关闭</el-button>
        </template>
        <template v-else>
          <el-button text @click="dismissFeedbackForever">不再提示</el-button>
          <el-button @click="closeFeedback">下次再说</el-button>
        </template>
      </template>
    </el-dialog>

    <el-dialog v-model="authDialog" title="需要访问令牌" width="420px"
               :close-on-click-modal="false">
      <p style="margin:0 0 6px;">这个调试台开启了鉴权，请输入访问令牌。</p>
      <p style="margin:0 0 10px;">没有令牌？去 <a href="https://seerinfo.yuyuqaq.cn/ad" target="_blank" rel="noopener">令牌获取页</a> 免费领取；问题反馈：<a href="mailto:feedback@scut.email">feedback@scut.email</a></p>
      <el-input v-model="tokenInput" placeholder="访问令牌" show-password
                @keyup.enter="saveToken" />
      <template #footer>
        <el-button @click="authDialog = false">取消</el-button>
        <el-button type="primary" @click="saveToken">保存并刷新</el-button>
      </template>
    </el-dialog>
  </el-container>
</template>
