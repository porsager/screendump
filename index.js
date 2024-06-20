const screendump = require('./build/Release/screendump.node')

module.exports = async function capture({ display = -1, scale = 1, quality = 75 } = {}) {
  return new Promise((resolve, reject) => {
    screendump.capture(display, scale, quality, (error, x) => error ? reject(error) : resolve(x))
  })
}
