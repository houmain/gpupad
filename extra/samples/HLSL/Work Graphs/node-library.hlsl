struct InputRecord
{
    uint id;
    uint value;
};

struct OutputRecord
{
    uint index;
    uint value;
};

cbuffer Settings : register(b0)
{
    uint singleRecord;
};

RWStructuredBuffer<uint> Results : register(u0);

[Shader("node")]
[NodeLaunch("thread")]
[NodeIsProgramEntry]
void produce(ThreadNodeInputRecord<InputRecord> input,
    [MaxRecords(2)] NodeOutput<OutputRecord> consume)
{
    uint id = input.Get().id;
    uint value = input.Get().value;
    uint count = singleRecord ? 1 : 1 + (value & 1);
    ThreadNodeOutputRecords<OutputRecord> records =
        consume.GetThreadNodeOutputRecords(count);
    for (uint k = 0; k < count; ++k) {
        records[k].index = 2 * id + k;
        records[k].value = (value + 1) * 10 + k;
    }
    records.OutputComplete();
}

[Shader("node")]
[NodeLaunch("thread")]
void consume(ThreadNodeInputRecord<OutputRecord> input)
{
    Results[input.Get().index] = input.Get().value;
}
