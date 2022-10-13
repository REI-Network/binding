module.exports = require("bindings")("evm-binding");

export type LastBlockHashesLoader = () => (string | Buffer)[];

export type BlockHeader = {
  parentHash?: string | Buffer;
  timestamp?: number;
  author?: string;
  transactionsRoot?: string | Buffer;
  receiptsRoot?: string | Buffer;
  sha3Uncles?: string | Buffer;
  stateRoot?: string | Buffer;
  gasUsed?: string | number;
  number?: number;
  gasLimit?: string | number;
  extraData?: string | Buffer;
  logBloom?: string | Buffer;
  difficulty?: string | number;
};

export type Transaction = {
  value?: string | number;
  gasPrice?: string | number;
  gas?: string | number;
  data?: string | Buffer;
  nonce?: string | number;
  from?: string;
  to?: string;
};

export type AccessList = [string, string[]][];

export type Message = {
  caller: string;
  to?: string;
  value: string | number;
  gasLimit: string | number;
  data: Buffer;
  isStatic: boolean;
  baseFee: string | number;
  gasPrice: string | number;
  isCreation: boolean;
  isUpgrade: boolean;
  clearStorage: boolean;
  clearEmptyAccount: boolean;
  author?: string;
  accessList?: AccessList;
};

export type ExecutionResult = {
  gasUsed: string;
  excepted?: { error: string };
  newAddress?: string;
  output: string;
  gasRefunded: string;
};

export type Log = {
  address: string;
  topics: string[];
  data: string;
};

export type TransactionReceipt = {
  logs: Log[];
  bloom: string;
  cumulativeGasUsed: string;
  status?: 0 | 1;
  stateRoot?: string;
};

export declare const init: () => void;

export declare class JSEVMBinding {
  /**
   * Construct a new JSEVMBinding object.
   * @param leveldb - External level db object
   * @param chainID - Blockchain id
   */
  constructor(leveldb: any, chainID: number);

  /**
   * Get chain id.
   */
  chainID(): number;

  /**
   * Force set hardfork by name
   * @param name - Hardfork name
   */
  setHardfork(name: string);

  /**
   * Reset hardfork
   */
  resetHardfork();

  /**
   * Initialize genesis state.
   * @param addresses - An array containing all addresses
   * @param balances - An array containing the balances of all addresses
   */
  genesis(addresses: string[], balances: (string | number)[]): string;

  /**
   * Execute transaction.
   * @param stateRoot - Previous state root hash
   * @param header - RLP encoded block header or header object
   * @param tx - RLP encoded transaction or transaction object
   * @param gasUsed - Gas used
   * @param loader - A function used to load block hash
   */
  runTx(
    stateRoot: string,
    header: Buffer | BlockHeader,
    tx: Buffer | Transaction,
    gasUsed: string | number,
    loader: LastBlockHashesLoader
  ): {
    stateRoot: string;
    result: ExecutionResult;
    receipt: TransactionReceipt;
  };

  /**
   * Execute call.
   * @param stateRoot - Previous state root hash
   * @param header - RLP encoded block header or header object
   * @param tx - RLP encoded transaction or transaction object
   * @param gasUsed - Gas used
   * @param loader - A function used to load block hash
   */
  runCall(
    stateRoot: string,
    header: Buffer | BlockHeader,
    tx: Buffer | Transaction,
    gasUsed: string | number,
    loader: LastBlockHashesLoader
  ): string;

  /**
   * Execute message.
   * @param stateRoot - Previous state root hash
   * @param header - RLP encoded block header or header object
   * @param message - Message object
   * @param gasUsed - Gas used
   * @param loader - A function used to load block hash
   */
  runMessage(
    stateRoot: string,
    header: Buffer | BlockHeader,
    message: Message,
    gasUsed: string | number,
    loader: LastBlockHashesLoader
  ): {
    stateRoot: string;
    result: ExecutionResult;
    logs: Log[];
  };
}
