import { createApp } from 'vue'
import ElementPlus from 'element-plus'
import zhCn from 'element-plus/es/locale/lang/zh-cn'
import 'element-plus/dist/index.css'
import * as Icons from '@element-plus/icons-vue'
import App from './App.vue'
import './styles.css'

const app = createApp(App)
app.use(ElementPlus, { locale: zhCn })
for (const [name, comp] of Object.entries(Icons)) {
  app.component(name, comp)
}
app.mount('#app')
