#ifndef PREPARECONTEXT_H
#define PREPARECONTEXT_H

#include "MessageList.h"
#include "Singletons.h"
#include "FileCache.h"
#include "session/SessionModel.h"
#include <QSet>

class EditorManager;

struct PrepareContext
{
    SessionModel &session;
    QSet<ItemId> &usedItems;
    MessageList &messages;

    PrepareContext(QSet<ItemId> *usedItems, MessageList *messages)
        : session(Singletons::sessionModel())
        , usedItems(*usedItems)
        , messages(*messages) { }
};

// updates list to contain the same elements as the update
// keeps the original elements when identical
// returns whether elements were added/removed
template<typename T>
bool updateList(std::vector<T> &list, std::vector<T> &&update)
{
    auto modified = false;
    for (auto &element : update) {
        auto it = std::find(list.begin(), list.end(), element);
        if (it != list.end()) {
            element = std::move(*it);
            list.erase(it);
        }
        else {
            modified = true;
        }
    }
    if (!list.empty())
        modified = true;
    list = std::move(update);
    return modified;
}

template<typename T, typename S, typename... Args>
int addOnce(std::vector<T> &list, const S &item, Args&&... args)
{
    auto element = T(item, args...);
    auto it = std::find(begin(list), end(list), element);
    if (it == end(list))
        it = list.insert(it, std::move(element));
    return std::distance(begin(list), it);
}

#endif // PREPARECONTEXT_H
