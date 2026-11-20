// 构建期守卫：模板属性里混入中文弯引号（“”）会让 Safari 的 setAttribute 抛
// InvalidCharacterError（Chrome 宽松不报）→ 整页白屏。2026-09-28 事故产物。
// 用法：npm run build 时自动先跑；也可单独 node scripts/check-curly-attrs.mjs
import { readdirSync, readFileSync, statSync } from 'node:fs'
import { join } from 'node:path'

const bad = []
const ROOT = process.cwd()

function checkFile(path, content) {
  content.split('\n').forEach((line, i) => {
    // 只拦"弯引号作属性定界符"（=两侧贴弯引号）；正文里的中文引号不动
    if (/=(“|”)/.test(line) || /(“|”)=/.test(line)) {
      bad.push(`${path}:${i + 1}  ${line.trim().slice(0, 80)}`)
    }
  })
}

function walk(dir) {
  for (const name of readdirSync(dir)) {
    const p = join(dir, name)
    const st = statSync(p)
    if (st.isDirectory()) {
      walk(p)
    } else if (name.endsWith('.vue') || name.endsWith('.html')) {
      checkFile(p, readFileSync(p, 'utf8'))
    }
  }
}

walk(join(ROOT, 'src'))
checkFile(join(ROOT, 'index.html'), readFileSync(join(ROOT, 'index.html'), 'utf8'))

if (bad.length) {
  console.error('❌ 模板属性引号混入中文弯引号（Safari 会白屏）：\n' + bad.join('\n'))
  process.exit(1)
}
console.log('✓ 弯引号属性检查通过')
