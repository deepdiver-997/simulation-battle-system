import { defineConfig } from 'vite'
import vue from '@vitejs/plugin-vue'

// 开发期把 API/WS 代理到网关（网关默认 8070）；构建产物 dist/ 由网关直接托管。
export default defineConfig({
  plugins: [vue()],
  // 手机/旧 webview 直连局域网控制台：默认 target 是 ES2020（可选链等），旧内核解析不了
  // 直接白屏——降到 es2015（用户 2026-09-28：手机空白页，桌面换 Chrome 才好 = 同因）。
  build: { target: 'es2015' },
  server: {
    port: 5173,
    proxy: {
      '/api': { target: 'http://127.0.0.1:8070', changeOrigin: true },
      '/ws': { target: 'ws://127.0.0.1:8070', ws: true },
    },
  },
})
