#pragma once

#include <string>
#include <unordered_map>
#include <DirectXMath.h>

// 面の各頂点を構成するインデックス
struct FaceIndex
{
    int v;  // 位置
    int vt; // テクスチャ座標
    int vn; // 法線

    bool operator==(const FaceIndex& other) const
    {
        return v == other.v && vt == other.vt && vn == other.vn;
    }
};

// ハッシュ値を生成する関数
namespace std
{
    template <>
    struct hash<FaceIndex>
    {
        size_t operator()(const FaceIndex& f) const
        {
            size_t h1 = std::hash<int>()(f.v);
            size_t h2 = std::hash<int>()(f.vt);
            size_t h3 = std::hash<int>()(f.vn);

            // ハッシュ合成
            return h1 ^ (h2 << 1) ^ (h3 << 2);
        }
    };
}

// 面（三角形）
struct Face
{
    FaceIndex faceIndices[3];
};

// サブメッシュ
struct SubMesh
{
    std::string material;       // マテリアル名
    std::vector<Face> faces;    // 面（三角形）情報
};

// メッシュ
struct Mesh
{
    std::vector<SubMesh> subMeshs;  // サブメッシュ
};

// obj形式の情報取得用構造体
struct Object
{
    std::string mtllib;                         // マテリアルファイル名
    std::vector<DirectX::XMFLOAT3> positions;   // 位置
    std::vector<DirectX::XMFLOAT3> normals;     // 法線
    std::vector<DirectX::XMFLOAT2> texcoords;   // テクスチャ座標
    std::vector<Mesh> meshes;                   // メッシュ
};

// マテリアル
struct Material
{
    DirectX::XMFLOAT3 ambientColor;     // アンビエント色
    DirectX::XMFLOAT3 diffuseColor;     // ディフューズ色
    DirectX::XMFLOAT3 specularColor;    // スペキュラー色
    float specularPower;                // スペキュラーパワー
    DirectX::XMFLOAT3 emissiveColor;    // エミッシブ色
    int32_t textureIndex;               // テクスチャインデックス

    Material()
        : ambientColor{ 1.0f, 1.0f, 1.0f }
        , diffuseColor{ 1.0f, 1.0f, 1.0f }
        , specularColor{ 1.0f, 1.0f, 1.0f }
        , specularPower{ 100.0f }
        , emissiveColor{ 0.0f, 0.0f, 0.0f }
        , textureIndex{ -1 }
    {
    }
};

// メッシュ情報
struct MeshInfo
{
    uint32_t materialIndex;     // マテリアルインデックス
    uint32_t materialNameIndex; // マテリアル名インデックス
    uint32_t startIndex;        // スタートインデックス  
    uint32_t primCount;         // プリミティブ数
};

// 頂点情報
struct VertexPositionNormalTextureTangent
{
    DirectX::XMFLOAT3 position;    // 位置
    DirectX::XMFLOAT3 normal;      // 法線
    DirectX::XMFLOAT2 texcoord;    // テクスチャ座標
    DirectX::XMFLOAT4 tangent;     // xyz = 接線, w = 従接線の向きを調整（1,-1)
};
