#include "Animation/PointDefinition.h"
#include "Animation/Track.h"
#include "Animation/Easings.h"
#include "TLogger.h"

using namespace NEVector;

Vector4 v4lerp(Vector4 a, Vector4 b, float t) {
    return Vector4(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t);
}

Vector3 SmoothVectorLerp(std::vector<PointData>& points, int a, int b, float time) {
    // Catmull-Rom Spline
    Vector3 p0 = a - 1 < 0 ? points[a].point : points[a - 1].point;
    Vector3 p1 = points[a].point;
    Vector3 p2 = points[b].point;
    Vector3 p3 = b + 1 > points.size() - 1 ? points[b].point : points[b + 1].point;

    float t = time;

    float tt = t * t;
    float ttt = tt * t;

    float q0 = -ttt + (2.0f * tt) - t;
    float q1 = (3.0f * ttt) - (5.0f * tt) + 2.0f;
    float q2 = (-3.0f * ttt) + (4.0f * tt) + t;
    float q3 = ttt - tt;

    Vector3 c = 0.5f * ((p0 * q0) + (p1 * q1) + (p2 * q2) + (p3 * q3));

    return c;
}

void PointDefinition::SearchIndex(float time, PropertyType propertyType, int& l, int& r) {
    l = 0;
    r = points.size();

    while (l < r - 1) {
        int m = (l + r) / 2;
        float pointTime = 0;
        switch (propertyType) {
        case PropertyType::linear:
            pointTime = points[m].linearPoint.y;
            break;

        case PropertyType::quaternion:
        case PropertyType::vector3:
            pointTime = points[m].point.w;
            break;
        
        case PropertyType::vector4:
            pointTime = points[m].vector4Point.v;
        }

        if (pointTime < time) {
            l = m;
        } else {
            r = m;
        }
    }
}

PointDefinition::PointDefinition(const rapidjson::Value& value) {
    for (int i = 0; i < value.Size(); i++) {
        const rapidjson::Value& rawPoint = value[i];

        std::vector<float> copiedList;
        bool spline = false;
        Functions easing = Functions::easeLinear;

        for (int j = 0; j < rawPoint.Size(); j++) {
            const rapidjson::Value& rawPointItem = rawPoint[j];

            switch (rawPointItem.GetType()) {
            case rapidjson::kNumberType:
                copiedList.push_back(rawPointItem.GetFloat());
                break;
            case rapidjson::kStringType: {
                std::string flag(rawPointItem.GetString());
                if (flag.starts_with("ease")) {
                    easing = FunctionFromStr(flag);
                } else if (flag == "splineCatmullRom") {
                    spline = true;
                }
                break;
            }
            default:
                // TODO: Handle wrong types
                break;
            }
        }

        int numNums = copiedList.size();
        if (numNums == 2) {
            Vector2 vec = Vector2(copiedList[0], copiedList[1]);
            points.push_back(PointData(vec, easing));
        } else if (numNums == 4) {
            Vector4 vec = Vector4(copiedList[0], copiedList[1], copiedList[2], copiedList[3]);
            points.push_back(PointData(vec, easing, spline));
        } else {
            Vector5 vec = Vector5(copiedList[0], copiedList[1], copiedList[2], copiedList[3], copiedList[4]);
            points.push_back(PointData(vec, easing));
        }
    }
}

Vector3 PointDefinition::Interpolate(float time) {
    if (points.size() == 0) {
        return Vector3::zero();
    }

    if (points[0].point.w >= time) {
        return points[0].point;
    }

    if (points[points.size() - 1].point.w <= time) {
        return points[points.size() - 1].point;
    }

    int l;
    int r;
    SearchIndex(time, PropertyType::vector3, l, r);

    float normalTime = (time - points[l].point.w) / (points[r].point.w - points[l].point.w);
    normalTime = Easings::Interpolate(normalTime, points[r].easing);
    if (points[r].smooth) {
        return SmoothVectorLerp(points, l, r, normalTime);
    } else {
        return Vector3::LerpUnclamped(points[l].point, points[r].point, normalTime);
    }
}

Quaternion PointDefinition::InterpolateQuaternion(float time) {
    if (points.size() == 0) {
        return Quaternion::get_identity();
    }

    if (points[0].point.w >= time) {
        return Quaternion::Euler(points[0].point);
    }

    if (points[points.size() - 1].point.w <= time) {
        return Quaternion::Euler(points[points.size() - 1].point);
    }

    int l;
    int r;
    SearchIndex(time, PropertyType::quaternion, l, r);

    Quaternion quaternionOne = Quaternion::Euler(points[l].point);
    Quaternion quaternionTwo = Quaternion::Euler(points[r].point);
    float normalTime = (time - points[l].point.w) / (points[r].point.w - points[l].point.w);
    normalTime = Easings::Interpolate(normalTime, points[r].easing);
    return Quaternion::SlerpUnclamped(quaternionOne, quaternionTwo, normalTime);
}

float PointDefinition::InterpolateLinear(float time) {
    if (points.size() == 0) {
        return 0;
    }

    if (points[0].linearPoint.y >= time) {
        return points[0].linearPoint.x;
    }

    if (points[points.size() - 1].linearPoint.y <= time) {
        return points[points.size() - 1].linearPoint.x;
    }

    int l;
    int r;
    SearchIndex(time, PropertyType::linear, l, r);

    float normalTime = (time - points[l].linearPoint.y) / (points[r].linearPoint.y - points[l].linearPoint.y);
    normalTime = Easings::Interpolate(normalTime, points[r].easing);
    return std::lerp(points[l].linearPoint.x, points[r].linearPoint.x, normalTime);
}

Vector4 PointDefinition::InterpolateVector4(float time) {
    if (points.size() == 0) {
        return Vector4{0,0,0,0};
    }

    if (points[0].vector4Point.v >= time) {
        return points[0].vector4Point;
    }

    if (points[points.size() - 1].vector4Point.v <= time) {
        return points[points.size() - 1].vector4Point;
    }

    int l;
    int r;
    SearchIndex(time, PropertyType::vector4, l, r);

    float normalTime = (time - points[l].vector4Point.v) / (points[r].vector4Point.v - points[l].vector4Point.v);
    normalTime = Easings::Interpolate(normalTime, points[r].easing);
    return v4lerp(points[l].vector4Point, points[r].vector4Point, normalTime);
}

void PointDefinitionManager::AddPoint(std::string pointDataName, PointDefinition pointData) {
    if (this->pointData.find(pointDataName) != this->pointData.end()) {
        TLogger::GetLogger().error("Duplicate point defintion name, %s could not be registered!", pointDataName.c_str());
    } else {
        this->pointData.emplace(pointDataName, pointData);
    }
} 