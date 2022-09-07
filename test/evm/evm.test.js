const evm = require('bindings')('evm-binding');

console.log('evm:', evm);

try {
    evm.run();
} catch(err) {
    console.log('error:', err);
}