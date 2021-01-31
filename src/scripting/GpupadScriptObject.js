
gpupad = function(gpupad) {
  const internal = {}

  const getItems = function() {
    if (typeof internal.root === 'undefined')
      internal.root = gpupad.getItems()
    return internal.root
  }

  const updateItems = function() {
    if (typeof internal.root !== 'undefined') {
      gpupad.updateItems(internal.root)
      delete internal.root
    }
  }

  const getItemByName = function(items, name) {
    if (items)
      for (let item of items)
        if (item.name === name)
          return item;
  }

  const getItem = function(keysString) {
    let items = getItems()
    if (keysString === '')
      return items
    const keys = keysString.split('/')
    for (;;) {
      const item = getItemByName(items, keys[0])
      if (!item || keys.length === 1)
        return item
      items = item.items
      keys.shift()
    }
  }

  const addItem = function(keysString, item) {
    const keys = keysString.split('/')
    const name = keys.pop()
    const parent = getItem(keys.join('/'))
    let items = parent
    if (parent !== getItems())
      items = parent.items = (parent.items || [])
    item.name = name
    items[items.length] = item
    return item
  }

  const deleteItem = function(keysString) {
    const item = getItem(keysString)
    const keys = keysString.split('/')
    const name = keys.pop()
    const parent = getItem(keys.join('/'))
    const items = (parent.items ? parent.items : parent)
    delete items[items.indexOf(item)]
    gpupad.deleteItem(item)
  }

  const gpupadProxy = {
    get(target, key) {
      switch(key) {
        case 'getItems': return getItems
        case 'updateItems': return updateItems
        case 'item': return getItem
        case 'addItem': return addItem
        case 'deleteItem': return deleteItem
      }
      return target[key]
    }
  }
  return new Proxy(gpupad, gpupadProxy)
}(gpupad)
