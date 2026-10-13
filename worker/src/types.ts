export interface Env {
  REGISTRY: DurableObjectNamespace;
  RATE: KVNamespace;
}

export interface FileMeta {
  name: string;
  size: number;
}

export interface TransferRequest {
  id: string;
  from: string;
  fromPeerId: string;
  files: FileMeta[];
  createdAt: number;
  status: "pending" | "accepted" | "rejected";
}
