#pragma once

#include <DirectXMath.h>

namespace ObjToImdl
{
    // マテリアル
    struct MaterialInfo
    {
        DirectX::XMFLOAT3 diffuseColor;     // ディフューズ色
        DirectX::XMFLOAT3 specularColor;    // スペキュラー色
        float specularPower;                // スペキュラーパワー
        DirectX::XMFLOAT3 emissiveColor;    // エミッシブ色
        int32_t textureIndex_BaseColor;     // テクスチャインデックス（ベースカラー）
        int32_t textureIndex_NormalMap;     // テクスチャインデックス（法線マップ）

        MaterialInfo()
            : diffuseColor{ 1.0f, 1.0f, 1.0f }
            , specularColor{ 1.0f, 1.0f, 1.0f }
            , specularPower{ 100.0f }
            , emissiveColor{ 0.0f, 0.0f, 0.0f }
            , textureIndex_BaseColor{ -1 }
            , textureIndex_NormalMap{ -1 }
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
}
