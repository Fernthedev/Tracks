#include "TLogger.h"
#include "THooks.h"
#include "AssociatedData.h"
#include "Animation/PointDefinition.h"
#include "beatsaber-hook/shared/utils/hooking.hpp"
#include "custom-json-data/shared/CustomBeatmapData.h"
#include "GlobalNamespace/BeatmapLineData.hpp"

#include "GlobalNamespace/BeatmapData.hpp"
#include "GlobalNamespace/BeatmapDataLoader.hpp"

using namespace GlobalNamespace;
using namespace TracksAD;

void TracksAD::readBeatmapDataAD(CustomJSONData::CustomBeatmapData *beatmapData) {
    static auto *customObstacleDataClass = classof(CustomJSONData::CustomObstacleData *);
    static auto *customNoteDataClass = classof(CustomJSONData::CustomNoteData *);

    BeatmapAssociatedData &beatmapAD = getBeatmapAD(beatmapData->customData);

    if (beatmapAD.valid) {
        return;
    }

    if (beatmapData->customData->value) {
        rapidjson::Value &customData = *beatmapData->customData->value;

        PointDefinitionManager pointDataManager;
        if (customData.HasMember("_pointDefinitions")) {
            const rapidjson::Value &pointDefinitions =
                customData["_pointDefinitions"];
            for (rapidjson::Value::ConstValueIterator itr =
                     pointDefinitions.Begin();
                 itr != pointDefinitions.End(); itr++) {
                std::string pointName = (*itr)["_name"].GetString();
                PointDefinition pointData((*itr)["_points"]);
                pointDataManager.AddPoint(pointName, pointData);
            }
        }
        TLogger::GetLogger().debug("Setting point definitions");
        beatmapAD.pointDefinitions = pointDataManager.pointData;
    }
    auto &tracks = beatmapAD.tracks;

    for (int i = 0; i < beatmapData->beatmapLinesData->Length(); i++) {
        BeatmapLineData *beatmapLineData = beatmapData->beatmapLinesData->values[i];
        for (int j = 0; j < beatmapLineData->beatmapObjectsData->size; j++) {
            BeatmapObjectData *beatmapObjectData =
                beatmapLineData->beatmapObjectsData->items->values[j];

            CustomJSONData::JSONWrapper *customDataWrapper;
            if (beatmapObjectData->klass == customObstacleDataClass) {
                auto obstacleData =
                    (CustomJSONData::CustomObstacleData *)beatmapObjectData;
                customDataWrapper = obstacleData->customData;
            } else if (beatmapObjectData->klass == customNoteDataClass) {
                auto noteData =
                    (CustomJSONData::CustomNoteData *)beatmapObjectData;
                customDataWrapper = noteData->customData;
            } else {
                continue;
            }

            if (customDataWrapper->value) {
                rapidjson::Value &customData = *customDataWrapper->value;
                BeatmapObjectAssociatedData &ad = getAD(customDataWrapper);
                if (customData.HasMember("_track")) {
                    std::string trackName = customData["_track"].GetString();
                    Track *track = &tracks[trackName];
                    ad.track = track;
                }
            }
        }
    }

    beatmapAD.valid = true;
}

MAKE_HOOK_MATCH(GetBeatmapDataFromBeatmapSaveData,
                &BeatmapDataLoader::GetBeatmapDataFromBeatmapSaveData, BeatmapData *,
                BeatmapDataLoader *self, List<BeatmapSaveData::NoteData *> *notesSaveData,
                List<BeatmapSaveData::WaypointData *> *waypointsSaveData,
                List<BeatmapSaveData::ObstacleData *> *obstaclesSaveData,
                List<BeatmapSaveData::EventData *> *eventsSaveData,
                BeatmapSaveData::SpecialEventKeywordFiltersData *evironmentSpecialEventFilterData,
                float startBpm, float shuffle, float shufflePeriod) {
    auto *result =
        reinterpret_cast<CustomJSONData::CustomBeatmapData *>(GetBeatmapDataFromBeatmapSaveData(
            self, notesSaveData, waypointsSaveData, obstaclesSaveData, eventsSaveData,
            evironmentSpecialEventFilterData, startBpm, shuffle, shufflePeriod));
    
    TracksAD::readBeatmapDataAD(result);

    return result;
}

void InstallBeatmapDataLoaderHooks(Logger &logger) {
    INSTALL_HOOK(logger, GetBeatmapDataFromBeatmapSaveData);
}

TInstallHooks(InstallBeatmapDataLoaderHooks);