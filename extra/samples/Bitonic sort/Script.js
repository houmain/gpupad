
// based on DirectX SDK's Direct3D 11 sample
// https://github.com/walbourn/directx-sdk-samples/tree/main/ComputeShaderSort11

if (!Session.items.Dynamic)
  Session.items.Dynamic = { }
let Dynamic = Session.items.Dynamic
Session.clearItem(Dynamic)

let Buffer1 = Session.items.Buffer1;
let Buffer2 = Session.items.Buffer2;
let BitonicSort =  Session.items.BitonicSort;
let MatrixTranspose =  Session.items.MatrixTranspose;

function SetConstants(level, levelMask, width, height) {
  Dynamic.items.uConstants = {
    type: 'Binding',
    editor: 'Expression4',
    values: [ level, levelMask, width, height ]
  }
}

function BindBuffer(name, buffer) {
  Dynamic.items[name] = {
    type: 'Binding',
    bindingType: 'Buffer',
    bufferId: buffer.id
  }
}

function Dispatch(program, workGroupsX, workGroupsY, workGroupsZ) {
  Dynamic.items[program.name] = {
    type: 'Call',
    callType: 'Compute',
    programId: program.id,
    workGroupsX: workGroupsX,
    workGroupsY: workGroupsY,
    workGroupsZ: workGroupsZ,
  }
}

// The number of elements to sort is limited to an even power of 2
// At minimum 8,192 elements - BITONIC_BLOCK_SIZE * TRANSPOSE_BLOCK_SIZE
// At maximum 262,144 elements - BITONIC_BLOCK_SIZE * BITONIC_BLOCK_SIZE
NUM_ELEMENTS = 512 * 512;
BITONIC_BLOCK_SIZE = 512;
TRANSPOSE_BLOCK_SIZE = 16;
MATRIX_WIDTH = BITONIC_BLOCK_SIZE;
MATRIX_HEIGHT = NUM_ELEMENTS / BITONIC_BLOCK_SIZE;

Buffer1.items.Block.rowCount = NUM_ELEMENTS;
Buffer2.items.Block.rowCount = NUM_ELEMENTS;

BindBuffer('bData', Buffer1);

// Sort the row data
for (let level = 2; level <= BITONIC_BLOCK_SIZE; level = level * 2) {
  SetConstants(level, level, MATRIX_HEIGHT, MATRIX_WIDTH);
  Dispatch(BitonicSort, NUM_ELEMENTS / BITONIC_BLOCK_SIZE, 1, 1);
}

// Then sort the rows and columns for the levels > than the block size
// Transpose. Sort the Columns. Transpose. Sort the Rows.
for (let level = BITONIC_BLOCK_SIZE * 2; level <= NUM_ELEMENTS; level = level * 2) {
  SetConstants(level / BITONIC_BLOCK_SIZE, (level & ~NUM_ELEMENTS) / BITONIC_BLOCK_SIZE, MATRIX_WIDTH, MATRIX_HEIGHT);

  // Transpose the data from buffer 1 into buffer 2
  BindBuffer('bData', Buffer2);
  BindBuffer('bInput', Buffer1);
  Dispatch(MatrixTranspose, MATRIX_WIDTH / TRANSPOSE_BLOCK_SIZE, MATRIX_HEIGHT / TRANSPOSE_BLOCK_SIZE, 1);

  // Sort the transposed column data
  Dispatch(BitonicSort, NUM_ELEMENTS / BITONIC_BLOCK_SIZE, 1, 1);

  SetConstants(BITONIC_BLOCK_SIZE, level, MATRIX_HEIGHT, MATRIX_WIDTH);

  // Transpose the data from buffer 2 back into buffer 1
  BindBuffer('bData', Buffer1);
  BindBuffer('bInput', Buffer2);
  Dispatch(MatrixTranspose, MATRIX_HEIGHT / TRANSPOSE_BLOCK_SIZE, MATRIX_WIDTH / TRANSPOSE_BLOCK_SIZE, 1 );

  // Sort the row data
  Dispatch(BitonicSort, NUM_ELEMENTS / BITONIC_BLOCK_SIZE, 1, 1);
}
