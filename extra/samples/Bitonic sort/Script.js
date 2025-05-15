
// based on DirectX SDK's Direct3D 11 sample
// https://github.com/walbourn/directx-sdk-samples/tree/main/ComputeShaderSort11

let Dynamic = app.session.findItem('Dynamic')
if (!Dynamic)
  Dynamic = app.session.insertItem({
    type: 'Group',
    name: 'Dynamic',
    dynamic: true,
  })
app.session.clearItems(Dynamic)

let Buffer1 = app.session.findItem('Buffer1')
let Buffer2 = app.session.findItem('Buffer2')
let Buffer1Block = app.session.findItem('Buffer1/Block')
let Buffer2Block = app.session.findItem('Buffer2/Block')
let BitonicSort = app.session.findItem('BitonicSort')
let MatrixTranspose = app.session.findItem('MatrixTranspose')

function SetConstants(level, levelMask, width, height) {
  app.session.insertItem(Dynamic, {
    type: 'Binding',    
    name: 'uConstants',
    editor: 'Expression4',
    values: [ level, levelMask, width, height ]
  })
}

function BindBuffer(name, buffer) {
  app.session.insertItem(Dynamic, {
    type: 'Binding',    
    name: name,
    bindingType: 'Buffer',
    bufferId: buffer.id
  })
}

function Dispatch(program, workGroupsX, workGroupsY, workGroupsZ) {
  app.session.insertItem(Dynamic, {
    type: 'Call',
    name: program.name,
    callType: 'Compute',
    programId: program.id,
    workGroupsX: workGroupsX,
    workGroupsY: workGroupsY,
    workGroupsZ: workGroupsZ,
  })
}

// The number of elements to sort is limited to an even power of 2
// At minimum 8,192 elements - BITONIC_BLOCK_SIZE * TRANSPOSE_BLOCK_SIZE
// At maximum 262,144 elements - BITONIC_BLOCK_SIZE * BITONIC_BLOCK_SIZE
NUM_ELEMENTS = 512 * 512;
BITONIC_BLOCK_SIZE = 512;
TRANSPOSE_BLOCK_SIZE = 16;
MATRIX_WIDTH = BITONIC_BLOCK_SIZE;
MATRIX_HEIGHT = NUM_ELEMENTS / BITONIC_BLOCK_SIZE;

Buffer1Block.rowCount = NUM_ELEMENTS;
Buffer2Block.rowCount = NUM_ELEMENTS;

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
