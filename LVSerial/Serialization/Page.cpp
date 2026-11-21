#include "Page.h"
#include "ObjectSerializer.h"
#include "ObjectUserData.h"
#include "ExtendedWidgets.h"
#include "AdvancedKeyboard.h"
#include "InteractionActions.h"

#include <unordered_map>
#include <unordered_set>

namespace Serialization
{
    json Page::ToJSON(std::vector<lv_obj_t*> objects)
    {
        std::unordered_map<lv_obj_t*, int> indices;
        for (auto* object : objects)
            if (object && indices.find(object) == indices.end())
                indices.emplace(object, static_cast<int>(indices.size()));

        json records = json::array();
        std::unordered_set<lv_obj_t*> document;
        for (const auto& entry : indices) document.insert(entry.first);
        DocumentSerializationScope scope(document);
        for (auto* object : objects)
        {
            const auto current = indices.find(object);
            if (current == indices.end() || current->second != static_cast<int>(records.size()))
                continue;
            auto record = ObjectSerializer::SerializeObject(object);
            auto events = InteractionActions::EventsForSave(object, indices);
            if (!events.empty() || record["base"].contains("events")) record["base"]["events"] = std::move(events);
            auto* host = lv_obj_get_parent(object);
            std::vector<uint32_t> hostPath;
            while (host && !indices.count(host))
            {
                hostPath.push_back(lv_obj_get_index(host));
                host = lv_obj_get_parent(host);
            }
            const auto parent = indices.find(host);
            record["oid"] = current->second;
            record["par"] = parent == indices.end() ? -1 : parent->second;
            if (parent != indices.end()) record["hostIndex"] = lv_obj_get_index(object);
            if (parent != indices.end() && !hostPath.empty())
            {
                std::reverse(hostPath.begin(), hostPath.end());
                record["hostPath"] = hostPath;
            }
            records.push_back(std::move(record));
        }
        return {{"formatVersion", 2}, {"lvglVersion", "9.6.0"}, {"obj", records}};
    }

    std::vector<lv_obj_t*> Page::FromJSON(json document)
    {
        std::vector<lv_obj_t*> objects;
        if (!document.is_object() || !document["obj"].is_array())
            return objects;

        struct ImportRollback
        {
            lv_obj_t* screen = lv_scr_act();
            std::unordered_set<lv_obj_t*> original;
            bool committed = false;
            ImportRollback()
            {
                for (uint32_t i = 0; i < lv_obj_get_child_count(screen); ++i)
                    original.insert(lv_obj_get_child(screen, i));
            }
            ~ImportRollback()
            {
                if (committed) return;
                for (int32_t i = static_cast<int32_t>(lv_obj_get_child_count(screen)) - 1; i >= 0; --i)
                {
                    auto* child = lv_obj_get_child(screen, i);
                    if (!original.count(child)) lv_obj_delete(child);
                }
            }
        } rollback;

        struct Entry
        {
            lv_obj_t* object;
            int parent;
            json hostPath;
            int hostIndex;
        };
        std::unordered_map<int, Entry> entries;
        std::vector<int> order;
        for (const auto& record : document["obj"])
        {
            if (!record.is_object() || !record.contains("oid") || !record["oid"].is_number_integer())
                continue;
            const int id = record["oid"].get<int>();
            if (id < 0 || entries.count(id))
                continue;
            if (auto* object = ObjectSerializer::DeserializeObject(record))
            {
                if(auto* data=static_cast<ObjectUserData*>(lv_obj_get_user_data(object)))data->objectID=static_cast<unsigned>(id);
                const int parent = record.value("par", -1);
                entries.emplace(id, Entry{object, parent, record.value("hostPath", json::array()), record.value("hostIndex", -1)});
                order.push_back(id);
                objects.push_back(object);
            }
        }

        for (const int id : order)
        {
            const auto entry = entries.at(id);
            auto parent = entries.find(entry.parent);
            if (parent == entries.end())
                continue;
            std::unordered_set<int> visited{id};
            bool cycle = false;
            for (auto ancestor = parent; ancestor != entries.end(); ancestor = entries.find(ancestor->second.parent))
            {
                if (!visited.insert(ancestor->first).second)
                {
                    cycle = true;
                    break;
                }
            }
            if (cycle)
                continue;
            auto* host = parent->second.object;
            for (const auto& index : entry.hostPath)
            {
                if (!index.is_number_unsigned() && !index.is_number_integer()) break;
                const auto child = index.get<uint32_t>();
                if (child >= lv_obj_get_child_count(host)) break;
                host = lv_obj_get_child(host, child);
            }
            if (entry.hostPath.empty()) host = ExtendedWidgets::ContentParent(host);
            lv_obj_set_parent(entry.object, host);
        }
        auto placement = order;
        std::stable_sort(placement.begin(), placement.end(), [&](int left, int right) { return entries.at(left).hostIndex < entries.at(right).hostIndex; });
        for (const int id : placement)
            if (entries.at(id).hostIndex >= 0) lv_obj_move_to_index(entries.at(id).object, entries.at(id).hostIndex);
        std::map<int, lv_obj_t*> targets;
        for (const auto& entry : entries) targets.emplace(entry.first, entry.second.object);
        AdvancedKeyboard::BindTargets(rollback.screen, targets);
        for (auto* object : objects) InteractionActions::ResolveReferences(object, targets);
        rollback.committed = true;
        return objects;
    }
}
