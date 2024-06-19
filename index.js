import { createRequire } from 'module'

const require = createRequire(import.meta.url)
const screendump = require('./build/Release/screendump.node')

export default capture

async function capture({ display = -1, scale = 1, quality = 75 } = {}) {
  return new Promise((resolve, reject) => {
    screendump.capture(display, scale, quality, (error, x) => error ? reject(error) : resolve(x))
  })
}
