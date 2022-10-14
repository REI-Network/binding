const bindings = require('bindings')

const tryList = [
    // node-waf and gyp_addon (a.k.a node-gyp)
    ['module_root', 'build', 'Debug', 'bindings'],
    ['module_root', 'build', 'Release', 'bindings'],
    // Production "Release" buildtype binary (meh...)
    ['module_root', 'compiled', 'platform', 'arch', 'bindings'],
]

module.exports = function (name) {
    return bindings({ bindings: name, try: tryList })
}