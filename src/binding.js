const bindings = require('bindings')

const releaseTryList = [
    // node-waf and gyp_addon (a.k.a node-gyp)
    ['module_root', 'build', 'Release', 'bindings'],
    // Production "Release" buildtype binary (meh...)
    ['module_root', 'compiled', 'platform', 'arch', 'bindings'],
]

const debugTryList = [
    ['module_root', 'build', 'Debug', 'bindings']
]

module.exports = function (name) {
    try {
        // load release binding
        return bindings({ bindings: name, try: releaseTryList })
    } catch {
        // load debug binding
        return bindings({ bindings: name + 'd', try: debugTryList })
    }
}