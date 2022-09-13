import bindings from "bindings";
import type { JSEVMGlobalInit, JSEVMBindingConstructor } from "./types";

const {
  init,
  JSEVMBinding,
}: { init: JSEVMGlobalInit; JSEVMBinding: JSEVMBindingConstructor } =
  bindings("evm-binding");

export { init, JSEVMBinding };
