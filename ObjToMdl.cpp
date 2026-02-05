// ObjToIma.cpp : このファイルには 'main' 関数が含まれています。プログラム実行の開始と終了がそこで行われます。
// wavefront形式のファイルを独自形式のモデルデータに変換するツール

// ------------------------------------------------------------ //
// モデルデータフォーマット
//
// テクスチャ名の数(uint32_t)
//      |テクスチャ名の文字数(uint32_t)   |*cnt
//      |テクスチャ名(char)               |
//
// マテリアル名の数(uint32_t)
//      |マテリアル名の文字数(uint32_t)   |*cnt
//      |マテリアル名(char)               |
//
// マテリアルの数(uint32_t)
//      マテリアル(Material*cnt)
//
// メッシュ情報の数(uint32_t)
//      メッシュ情報(MeshInfo*cnt)
//
// インデックス情報の数(uint32_t)
//      インデックス情報(uint16_t * cnt)
//
// 頂点情報の数(uint32_t)
//      頂点情報(VertexPositionNormalTextureTangent * cnt)
//
// ------------------------------------------------------------ //

#include "ObjToMdl.h"
#include <iostream>
#include <windows.h>
#include <vector>
#include <fstream>
#include "cxxopts.hpp"

using namespace DirectX;

// パス名付きファイル名のファイル名を取得する関数
static std::string GetFileNameOnly(const std::string& path)
{
    return std::filesystem::path(path).filename().string();
}

// ファイルから読み込む関数（XMFLOAT2）
static XMFLOAT2 ReadFloat2(std::istringstream& iss)
{
    XMFLOAT2 val = {};
    iss >> val.x >> val.y;
    return val;
}

// ファイルから読み込む関数（XMFLOAT3）
static XMFLOAT3 ReadFloat3(std::istringstream& iss)
{
    XMFLOAT3 val = {};
    iss >> val.x >> val.y >> val.z;
    return val;
}

// UTF-16 → UTF-8 変換
static std::string WStringToUtf8(const std::wstring& ws)
{
    int size = WideCharToMultiByte(
        CP_UTF8, 0,
        ws.c_str(), -1,
        nullptr, 0,
        nullptr, nullptr);

    std::string result(size - 1, 0);

    WideCharToMultiByte(
        CP_UTF8, 0,
        ws.c_str(), -1,
        result.data(), size,
        nullptr, nullptr);

    return result;
}

// ヘルプ表示
static void Help()
{
    std::cout <<
        "Usage:\n"
        "  ObjToMdl <input.obj> [-o output.mdl]\n\n"
        "Options:\n"
        "  -o, --output <file>   Output file\n"
        "  -h, --help            Show help\n";
}

// 引数から入力ファイル名と出力ファイル名を取得する関数
static int AnalyzeOption(int argc, char* argv[], std::string& input, std::string& output)
{
    // cxxoptsで引数解析
    cxxopts::Options options("ObjToMdl");
    options.add_options()
        ("input", "Input model file (.obj)",
            cxxopts::value<std::string>())
        ("o,output", "Output file",
            cxxopts::value<std::string>())
        ("h,help", "Show help");
    options.parse_positional({ "input" });

    try
    {
        auto result = options.parse(argc, argv);

        // -h,--help 指定された
        if (result.count("help"))
        {
            Help();
            return 0;
        }

        // 入力ファイル名
        input = result["input"].as<std::string>();

        // -o,-output 出力ファイル名
        if (result.count("output") == 0) {
            // 指定されていない場合は出力ファイル名は、入力ファイル名.mdlにする
            std::filesystem::path p(input);
            p.replace_extension(".mdl");
            output = p.string();
        }
        else
        {
            // 指定された
            output = result["output"].as<std::string>();
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        Help();
        return 1;
    }

    return 0;
}

// 面の各頂点を構成するインデックス取得関数
static std::vector<FaceIndex> ParseFaceLine(const std::string& line, Object& object)
{
    auto fixIndex = [&](int raw, int size) {
        if (raw > 0)  return raw - 1;
        if (raw < 0)  return size + raw;
        throw std::runtime_error("OBJ index cannot be zero");
    };

    std::istringstream iss(line);

    std::string type;
    iss >> type; // "f"

    std::vector<FaceIndex> result;

    std::string token;
    while (iss >> token)
    {
        FaceIndex idx{ -1, -1, -1 };

        std::istringstream tss(token);
        std::string s;

        // v
        if (std::getline(tss, s, '/')) idx.v = fixIndex(std::stoi(s), static_cast<int>(object.positions.size()));

        // vt
        if (std::getline(tss, s, '/'))
        {
            if (!s.empty()) idx.vt = fixIndex(std::stoi(s), static_cast<int>(object.texcoords.size()));
        }

        // vn
        if (std::getline(tss, s, '/'))
        {
            if (!s.empty()) idx.vn = fixIndex(std::stoi(s), static_cast<int>(object.normals.size()));
        }

        result.push_back(idx);
    }

    return result;
}

// objファイルの情報取得関数
static int AnalyzeObj(const char* fname, Object& object)
{
    // objファイルのオープン
    std::ifstream ifs(fname);

    if (!ifs)
    {
        // ファイルのオープン失敗
        std::cout << "Could not open " << fname << std::endl;
        return 1;
    }

    std::vector<Face>* pFace = nullptr;
    std::string object_name;

    std::string line;
    while (std::getline(ifs, line))
    {
        // 空行やコメントをスキップ
        if (line.empty() || line[0] == '#') continue;

        std::istringstream iss(line);

        // 先頭のトークン
        std::string type;
        iss >> type;

        // オブジェクト名
        if (type == "o")
        {
            iss >> object_name;
            object.meshes.emplace_back();
            pFace = nullptr;
        }

        // 頂点
        if (type == "v")
        {
            object.positions.push_back(ReadFloat3(iss));
        }

        // 法線
        else if (type == "vn")
        {
            object.normals.push_back(ReadFloat3(iss));
        }

        // テクスチャ座標
        else if (type == "vt")
        {
            // BlenderのV座標は上が＋
            XMFLOAT2 uv = ReadFloat2(iss);
            uv.y = 1.0f - uv.y;
            object.texcoords.push_back(uv);
        }

        // 面情報
        else if (type == "f")
        {
            // マテリアルがない
            if (pFace == nullptr)
            {
                std::cout << object_name << " has no material assigned." << std::endl;
                return 1;
            }

            // 面の各頂点を構成するインデックスを取得
            std::vector<FaceIndex> result = ParseFaceLine(line, object);

            // 四角形の場合は三角形２枚に置き換える
            for (size_t i = 0; i < result.size() - 2; i++)
            {
                // 時計回りが表
                Face face{ result[0], result[i + 2], result[i + 1] };
                pFace->push_back(face);
            }
        }

        // マテリアル名
        else if (type == "usemtl")
        {
            // メッシュを追加
            object.meshes.back().subMeshs.emplace_back();

            // マテリアル名
            std::string material;
            iss >> material;
            object.meshes.back().subMeshs.back().material = material;

            // 面を設定するポインタを更新
            pFace = &object.meshes.back().subMeshs.back().faces;
        }

        // マテリアルファイル名
        else if (type == "mtllib")
        {
            iss >> object.mtllib;
        }
    }

    return 0;
}

// テクスチャ名の登録関数
static int32_t RegisterTextureName(std::istringstream& iss,
    std::unordered_map<std::string, int32_t>& textureIndexMap,
    std::vector<std::string>& textures,
    int32_t& t_index
)
{
    // 最後のトークンをファイル名として取得
    std::string token, name;
    while (iss >> token)
    {
        name = token;
    }

    // エラー
    if (name.empty()) return -1;

    // パス名を除去
    name = GetFileNameOnly(name);

    // 既に同じテクスチャ名が登録済みの場合も考慮
    auto [it, inserted] = textureIndexMap.try_emplace(name, t_index);

    // 新しく挿入された
    if (inserted)
    {
        t_index++;
        textures.push_back(name);
    }

    return it->second;
}

// mtlファイルの情報取得関数
static int AnalyzeMtl( const char* fname,
                       std::vector<Material>& materials,
                       std::unordered_map<std::string, uint32_t>& materialIndexMap,
                       std::vector<std::string>& textures )
{
    // mtlファイルのオープン
    std::ifstream ifs(fname);

    if (!ifs)
    {
        // ファイルのオープン失敗
        std::cout << "Could not open " << fname << std::endl;
        return 1;
    }

    std::unordered_map<std::string, int32_t> textureIndexMap;

    uint32_t m_index = 0;
    int32_t t_index = 0;

    std::string line;
    while (std::getline(ifs, line))
    {
        // 空行やコメントをスキップ
        if (line.empty() || line[0] == '#') continue;

        std::istringstream iss(line);

        // 先頭のトークン
        std::string type;
        iss >> type;

        // マテリアルファイル名
        if (type == "newmtl")
        {
            std::string name;
            iss >> name;
            materials.resize(materials.size() + 1);
            materialIndexMap[name] = m_index;
            m_index++;
        }

        // アンビエント色
        else if (type == "Ka")
        {
            materials.back().ambientColor = ReadFloat3(iss);
        }

        // ディフューズ色
        else if (type == "Kd")
        {
            materials.back().diffuseColor = ReadFloat3(iss);
        }

        // スペキュラ色
        else if (type == "Ks")
        {
            materials.back().specularColor = ReadFloat3(iss);
        }

        // スペキュラパワー
        else if (type == "Ns")
        {
            iss >> materials.back().specularPower;
        }

        // エミッシブ色
        else if (type == "Ke")
        {
            materials.back().emissiveColor = ReadFloat3(iss);
        }

        // テクスチャ（ベースカラー）
        else if (type == "map_Kd")
        {
            if (!materials.empty())
            {
                materials.back().textureIndex_BaseColor = RegisterTextureName(iss, textureIndexMap, textures, t_index);
            }
        }

        // テクスチャ（法線マップ）
        else if (type == "map_Bump")
        {
            if (!materials.empty())
            {
                materials.back().textureIndex_NormalMap = RegisterTextureName(iss, textureIndexMap, textures, t_index);
            }
        }
    }

    return 0;
}

// 頂点データ作成関数
static VertexPositionNormalTextureTangent MakeVertex(Object& object, const FaceIndex& face)
{
    VertexPositionNormalTextureTangent v = {};

    v.position = object.positions[face.v];

    if (face.vn >= 0)
    {
        // 法線を正規化
        XMVECTOR n = XMLoadFloat3(&object.normals[face.vn]);
        n = XMVector3Normalize(n);
        XMStoreFloat3(&v.normal, n);
    }
    else
    {
        // ダミー
        v.normal = XMFLOAT3(0.0f, 0.0f, 1.0f);
    }

    v.texcoord = (face.vt >= 0) ? object.texcoords[face.vt] : XMFLOAT2(0.0f, 0.0f);

    return v;
}

static void CreateBufferData( Object& object, 
                              std::unordered_map<std::string, uint32_t>& materialIndexMap,
                              std::vector<MeshInfo>& meshInfo,
                              std::vector<VertexPositionNormalTextureTangent>& vertexBuffer,
                              std::vector<uint16_t>& indexBuffer )
{
    std::unordered_map<FaceIndex, uint16_t> indexMap;

    for (auto& mesh : object.meshes)
    {
        for (auto& subMesh : mesh.subMeshs)
        {
            // サブメッシュ情報
            MeshInfo data = {};
            auto it = materialIndexMap.find(subMesh.material);
            if (it == materialIndexMap.end()) throw std::runtime_error("Material not found: " + subMesh.material);
            data.materialIndex = it->second;                                // マテリアルインデックス
            data.materialNameIndex = it->second;                            // マテリアル名インデックス
            data.startIndex = static_cast<uint32_t>(indexBuffer.size());    // スタートインデックス
            data.primCount = static_cast<uint32_t>(subMesh.faces.size());   // プリミティブ数
            meshInfo.push_back(data);

            for (auto& face : subMesh.faces)
            {
                for (int i = 0; i < 3; i++)
                {
                    auto it = indexMap.find(face.faceIndices[i]);

                    if (it == indexMap.end())
                    {
                        // 新規頂点
                        uint16_t newIndex = static_cast<uint16_t>(vertexBuffer.size());
                        indexMap[face.faceIndices[i]] = newIndex;

                        vertexBuffer.push_back(MakeVertex(object, face.faceIndices[i]));
                        indexBuffer.push_back(newIndex);
                    }
                    else
                    {
                        // 既存頂点
                        indexBuffer.push_back(it->second);
                    }
                }
            }
        }
    }
}

// ファイルへの出力関数
static int OutputMdl( const char* fname,
                      std::vector<Material>& materials,
                      std::vector<MeshInfo>& meshInfo,
                      std::vector<std::string>& materialNames,
                      std::vector<std::string>& textures,
                      std::vector<VertexPositionNormalTextureTangent>& vertexBuffer,
                      std::vector<uint16_t>& indexBuffer )
{
    // mdlファイルのオープン
    std::ofstream ofs(fname, std::ios::binary);

    if (!ofs.is_open())
    {
        // ファイルのオープン失敗
        std::cout << "Could not open " << fname << std::endl;
        return 1;
    }

    // テクスチャ
    uint32_t texture_cnt = static_cast<uint32_t>(textures.size());
    ofs.write(reinterpret_cast<const char*>(&texture_cnt), sizeof(texture_cnt));
    for (const auto& texture : textures)
    {
        uint32_t size = static_cast<uint32_t>(texture.size());
        ofs.write(reinterpret_cast<const char*>(&size), sizeof(size));
        ofs.write(texture.data(), size);
    }

    // マテリアル名テーブル
    uint32_t materialName_cnt = static_cast<uint32_t>(materialNames.size());
    ofs.write(reinterpret_cast<const char*>(&materialName_cnt), sizeof(materialName_cnt));
    for (const auto& name : materialNames)
    {
        uint32_t len = static_cast<uint32_t>(name.size());
        ofs.write(reinterpret_cast<const char*>(&len), sizeof(len));
        ofs.write(name.data(), len);
    }

    // マテリアル
    uint32_t material_cnt = static_cast<uint32_t>(materials.size());
    ofs.write(reinterpret_cast<const char*>(&material_cnt), sizeof(material_cnt));
    ofs.write(reinterpret_cast<const char*>(materials.data()), sizeof(Material) * material_cnt);

    // メッシュ情報
    uint32_t mesh_cnt = static_cast<uint32_t>(meshInfo.size());
    ofs.write(reinterpret_cast<const char*>(&mesh_cnt), sizeof(mesh_cnt));
    ofs.write(reinterpret_cast<const char*>(meshInfo.data()), sizeof(MeshInfo) * mesh_cnt);

    // インデックス
    uint32_t index_cnt = static_cast<uint32_t>(indexBuffer.size());
    ofs.write(reinterpret_cast<const char*>(&index_cnt), sizeof(index_cnt));
    ofs.write(reinterpret_cast<const char*>(indexBuffer.data()), sizeof(uint16_t) * index_cnt);

    // 頂点
    uint32_t vertex_cnt = static_cast<uint32_t>(vertexBuffer.size());
    ofs.write(reinterpret_cast<const char*>(&vertex_cnt), sizeof(vertex_cnt));
    ofs.write(reinterpret_cast<const char*>(vertexBuffer.data()), sizeof(VertexPositionNormalTextureTangent) * vertex_cnt);

    return 0;
}

// パス名を取得する関数
static std::string GetDirectoryPath(const std::string& filepath)
{
    std::filesystem::path p(filepath);
    return p.parent_path().string();
}

// パス名とファイル名を結合する関数
static std::string JoinPath(const std::string& path, const std::string& filename)
{
    std::filesystem::path p(path);
    p /= filename;   // パス結合
    return p.string();
}

// 頂点データに接線を追加する関数
static void GenerateTangents(
    std::vector<VertexPositionNormalTextureTangent>& vertices,
    const std::vector<uint16_t>& indices)
{
    std::vector<XMFLOAT3> tanAccum(vertices.size(), { 0,0,0 });
    std::vector<XMFLOAT3> bitanAccum(vertices.size(), { 0,0,0 });

    auto add = [&](uint32_t idx, const XMFLOAT3& t, const XMFLOAT3& b)
        {
            tanAccum[idx].x += t.x;
            tanAccum[idx].y += t.y;
            tanAccum[idx].z += t.z;

            bitanAccum[idx].x += b.x;
            bitanAccum[idx].y += b.y;
            bitanAccum[idx].z += b.z;
        };

    auto set = [&](uint32_t idx, const XMFLOAT3& t, const XMFLOAT3& b)
        {
            tanAccum[idx] = t;
            bitanAccum[idx] = b;
        };

    // 三角形の各頂点の法線が同じ向きならフラットシェーディングの面と判定
    auto isFlatFace = [&](uint32_t i0, uint32_t i1, uint32_t i2)
        {
            XMVECTOR n0 = XMLoadFloat3(&vertices[i0].normal);
            XMVECTOR n1 = XMLoadFloat3(&vertices[i1].normal);
            XMVECTOR n2 = XMLoadFloat3(&vertices[i2].normal);

            float d01 = XMVectorGetX(XMVector3Dot(n0, n1));
            float d12 = XMVectorGetX(XMVector3Dot(n1, n2));

            return d01 > 0.999f && d12 > 0.999f;
        };

    // ---- 三角形ごと ----
    for (size_t i = 0; i + 2 < indices.size(); i += 3)
    {
        uint32_t i0 = indices[i + 0];
        uint32_t i1 = indices[i + 1];
        uint32_t i2 = indices[i + 2];

        auto& v0 = vertices[i0];
        auto& v1 = vertices[i1];
        auto& v2 = vertices[i2];

        XMVECTOR p0 = XMLoadFloat3(&v0.position);
        XMVECTOR p1 = XMLoadFloat3(&v1.position);
        XMVECTOR p2 = XMLoadFloat3(&v2.position);

        float du1 = v1.texcoord.x - v0.texcoord.x;
        float dv1 = v1.texcoord.y - v0.texcoord.y;
        float du2 = v2.texcoord.x - v0.texcoord.x;
        float dv2 = v2.texcoord.y - v0.texcoord.y;

        float denom = du1 * dv2 - du2 * dv1;
        if (fabs(denom) < 1e-6f)
            continue;

        float f = 1.0f / denom;

        XMVECTOR e1 = p1 - p0;
        XMVECTOR e2 = p2 - p0;

        XMVECTOR T = (e1 * dv2 - e2 * dv1) * f;
        XMVECTOR B = (e2 * du1 - e1 * du2) * f;

        XMFLOAT3 t, b;
        XMStoreFloat3(&t, T);
        XMStoreFloat3(&b, B);

        bool flat = isFlatFace(i0, i1, i2);

        if (flat)
        {
            // フラット：上書き
            set(i0, t, b);
            set(i1, t, b);
            set(i2, t, b);
        }
        else
        {
            // スムーズ：加算
            add(i0, t, b);
            add(i1, t, b);
            add(i2, t, b);
        }
    }

    // ---- 正規化 & handedness ----
    for (size_t i = 0; i < vertices.size(); i++)
    {
        XMVECTOR N = XMLoadFloat3(&vertices[i].normal);
        XMVECTOR T = XMLoadFloat3(&tanAccum[i]);
        XMVECTOR B = XMLoadFloat3(&bitanAccum[i]);

        T = XMVector3Normalize(T - N * XMVector3Dot(N, T));

        float w = (XMVectorGetX(
            XMVector3Dot(XMVector3Cross(N, T), B)) < 0.0f)
            ? -1.0f : 1.0f;

        XMFLOAT3 t;
        XMStoreFloat3(&t, T);
        vertices[i].tangent = { t.x, t.y, t.z, w };
    }
}

// メイン
int wmain(int argc, wchar_t* wargv[])
{
    std::vector<std::string> args;
    std::vector<char*> argv;

    // 文字コードをUTF-8へ変換する
    for (int i = 0; i < argc; ++i)
    {
        args.push_back(WStringToUtf8(wargv[i]));
    }

    for (auto& s : args)
    {
        argv.push_back(s.data());
    }

    std::string input, output;

    // 入力ファイル名と出力ファイル名を取得
    if (AnalyzeOption(argc, argv.data(), input, output)) return 1;

    // ----- 情報取得 ----- //

    Object object;
 
    // objファイルの情報取得
    if (AnalyzeObj(input.c_str(), object)) return 1;

    // mtlファイルの情報取得
    object.mtllib = JoinPath(GetDirectoryPath(input), object.mtllib);

    // マテリアルを取得
    std::vector<Material> materials;
    std::unordered_map<std::string, uint32_t> materialIndexMap;
    std::vector<std::string> textures;
    if (AnalyzeMtl(object.mtllib.c_str(), materials, materialIndexMap, textures)) return 1;

    // マテリアル名の配列を作成
    std::vector<std::string> materialNames(materials.size());
    for (const auto& [name, index] : materialIndexMap)
    {
        materialNames[index] = name;
    }

    // 頂点、インデックスを取得
    std::vector<MeshInfo> meshInfo;
    std::vector<VertexPositionNormalTextureTangent> vertexBuffer;
    std::vector<uint16_t> indexBuffer;
    CreateBufferData(object, materialIndexMap, meshInfo, vertexBuffer, indexBuffer);

    // 頂点データに接線を追加
    GenerateTangents(vertexBuffer, indexBuffer);

    // ----- 書き出し ----- //

    if (OutputMdl(output.c_str(), materials, meshInfo, materialNames, textures, vertexBuffer, indexBuffer)) return 1;

    return 0;
}

