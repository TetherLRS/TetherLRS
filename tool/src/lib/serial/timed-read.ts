// Reads with a deadline. A read given up at its deadline stays pending and takes the next bytes, so racing a fresh
// read against a timer each time lost the start of every answer that followed a timeout: here the next call waits on
// that same read instead.

export function timedReader(reader: ReadableStreamDefaultReader<Uint8Array>) {
  let pending: Promise<ReadableStreamReadResult<Uint8Array>> | undefined;
  return async (ms: number): Promise<ReadableStreamReadResult<Uint8Array> | undefined> => {
    pending ??= reader.read();
    const r = await Promise.race([pending, new Promise<undefined>((done) => setTimeout(() => done(undefined), Math.max(0, ms)))]);
    if (r) pending = undefined;
    return r;
  };
}
