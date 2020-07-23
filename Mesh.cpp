#include "Mesh.h"
#include "Utilities.h"
#include "FrameResource.h" // for Vertex

#include <iostream>
#include <map>
#include <vector>

#include <limits> // std::numeric_limits

using namespace std;
using namespace DirectX;


D3D12_INDEX_BUFFER_VIEW Mesh::IndexBufferView() const
{
    D3D12_INDEX_BUFFER_VIEW ibv;
    ibv.BufferLocation = IndexBufferGPU->GetGPUVirtualAddress();
    ibv.Format = IndexFormat;
    ibv.SizeInBytes = IndexBufferByteSize;

    return ibv;
}

D3D12_VERTEX_BUFFER_VIEW Mesh::VertexBufferView() const
{
    D3D12_VERTEX_BUFFER_VIEW vbv;
    vbv.BufferLocation = VertexBufferGPU->GetGPUVirtualAddress();
    vbv.StrideInBytes = VertexByteStride;
    vbv.SizeInBytes = VertexBufferByteSize;

    return vbv;
}

void Mesh::DisposeUploaders()
{
    VertexBufferUploader = nullptr;
    IndexBufferUploader = nullptr;
}

/*
Loads a mesh from an OBJ file, including submeshes. 

Returns an integer less than 0 on failure. Returns 0 for success.
*/
int Mesh::LoadOBJ(wstring filename, bool vertexIndexListsOnly = false)
{
    assert(mD3Device);
    assert(mCommandList);

    vector<XMFLOAT3> positions;
    vector<XMFLOAT2> texcoords;
    vector<XMFLOAT3> normals;
    map<string, int> verts_added;
    float boundingRadius = 0.0f;
    
    float maxX = (std::numeric_limits<float>::lowest)(), maxY = (std::numeric_limits<float>::lowest)(), maxZ = (std::numeric_limits<float>::lowest)(),
        minX = (std::numeric_limits<float>::max)(), minY = (std::numeric_limits<float>::max)(), minZ = (std::numeric_limits<float>::max)();
    
    vector<uint16_t> indices;
    vector<Vertex> vertices;

    UINT posOffset = 0;
    UINT texOffset = 0;
    UINT normOffset = 0;
    UINT vertexOffset = 0;

    string lastObject = "";

    string line;
    ifstream obj(filename);
    if (!obj.is_open())
        return -1;

    OutputDebugStringA("\n");
    while (getline(obj, line))
    {
        OutputDebugStringA(line.c_str());
        OutputDebugStringA("\n");
        
        /*
        We skip:
        '#': comment
        'm': mttlib
        's': something about smoothing
        'u': usemtl

        We process:
        'v': vertex position
        'vt': vertex texture coord
        'vn': vertex normal
        'f': face indices
        'o': object name
        */
        switch (line[0])
        {
        case 'v':
        case 'vt':
        case 'vn':
        case 'f':
        case 'o':
            break;
        default:
            continue; 
        }
        
        auto tokens = split(line, ' ');
        // Create new object
        if (tokens[0] == "o")
        {
            auto currObject = tokens[1];
            
            // Store & update previous object
            if (lastObject != "")
            {
                // Update last objects indexcount since we now know how many indices it has
                DrawArgs[lastObject].IndexCount = (UINT)indices.size() - DrawArgs[lastObject].IndexCount;

                // Fill out bounding box
                XMVECTOR pt1{ minX, minY, minZ };
                XMVECTOR pt2{ maxX, maxY, maxZ };
                BoundingBox::CreateFromPoints(DrawArgs[lastObject].Bounds, pt1, pt2);

                maxX = (std::numeric_limits<float>::lowest)(), maxY = (std::numeric_limits<float>::lowest)(), maxZ = (std::numeric_limits<float>::lowest)();
                minX = (std::numeric_limits<float>::max)(), minY = (std::numeric_limits<float>::max)(), minZ = (std::numeric_limits<float>::max)();
            }

            SubmeshGeometry sg;
            sg.BaseVertexLocation = 0;// vertices.size();
            sg.StartIndexLocation = (UINT)indices.size();
            sg.IndexCount = (UINT)indices.size(); // must be set at end of object read
            
            DrawArgs[currObject] = sg;
            lastObject = currObject;
        }

            
        // Vertex position. These come first, so we create a new vertex
        if (tokens[0] == "v")
        {
            auto x = (float)::atof(tokens[1].c_str());
            auto y = (float)::atof(tokens[2].c_str());
            auto z = (float)::atof(tokens[3].c_str());

            if (x < minX)
                minX = x;
            if (x > maxX)
                maxX = x;
            if (y < minY)
                minY = y;
            if (y > maxY)
                maxY = y;
            if (z < minZ)
                minZ = z;
            if (z > maxZ)
                maxZ = z;

            positions.push_back(DirectX::XMFLOAT3(x, y, z));
        }
        // Texture coordinate. All vertices constructed
        else if (tokens[0] == "vt")
            texcoords.push_back(XMFLOAT2((float)::atof(tokens[1].c_str()), (float)::atof(tokens[2].c_str())));
        else if (tokens[0] == "vn")
            normals.push_back(XMFLOAT3((float)::atof(tokens[1].c_str()), (float)::atof(tokens[2].c_str()), (float)::atof(tokens[3].c_str())));
        else if (tokens[0] == "f")
        {
            // At this point we have collected all necessary data to index. So we can finish constructing the vertices and the indices.

            // Face indices are 1-based in OBJ files
            // tokens now contains faces of the form vertex_idx / texture_idx / normal_idx 
            // we ASSUME both texture/normals are exported for now
                
            // there are 4 tokens - one is "f", then assuming a triangulated mesh there are 3 others with the indices
            for (size_t i = 1; i < tokens.size(); ++i)
            {
                // Check if we already have this vertex
                int vertex_id = -1;
                if (verts_added.count(tokens[i]))
                    vertex_id = verts_added[tokens[i]];
                else
                {
                    // Construct vertex
                    auto inds = split(tokens[i], '/');
                    
                    Vertex v;
                    v.Pos = positions[::atoi(inds[0].c_str()) - 1];
                    v.TexC = texcoords[::atoi(inds[1].c_str()) - 1];
                    v.Normal = normals[::atoi(inds[2].c_str()) - 1];

                    // Register that we have added it, with its index into the vertex array
                    verts_added[tokens[i]] = (int)vertices.size();
                    vertex_id = (int)vertices.size();
                    vertices.push_back(v);
                }

                // Now we have the vertex, so add it to the index buffer
                indices.push_back(vertex_id);
            }
        }
    }

    // End of file - fix the index count in the last object
    DrawArgs[lastObject].IndexCount = (UINT)indices.size() - DrawArgs[lastObject].IndexCount;

    // Fill out bounding box
    XMVECTOR pt1{ minX, minY, minZ };
    XMVECTOR pt2{ maxX, maxY, maxZ };
    BoundingBox::CreateFromPoints(DrawArgs[lastObject].Bounds, pt1, pt2);

    // Aaand close out
    obj.close();
    
    // We gots verts and indices, only remains to create & fill buffers
    const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
    const UINT ibByteSize = (UINT)indices.size() * sizeof(uint16_t);

    ThrowIfFailed(D3DCreateBlob(vbByteSize, &VertexBufferCPU));
    CopyMemory(VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

    ThrowIfFailed(D3DCreateBlob(ibByteSize, &IndexBufferCPU));
    CopyMemory(IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

    if (!createCPUBufferOnly)
    {
        VertexBufferGPU = Utilities::CreateDefaultBuffer(mD3Device.Get(), mCommandList.Get(), vertices.data(), vbByteSize, VertexBufferUploader);
        IndexBufferGPU = Utilities::CreateDefaultBuffer(mD3Device.Get(), mCommandList.Get(), indices.data(), ibByteSize, IndexBufferUploader);
    }

    VertexByteStride = sizeof(Vertex);
    VertexBufferByteSize = vbByteSize;
    IndexFormat = DXGI_FORMAT_R16_UINT;
    IndexBufferByteSize = ibByteSize;

    // Success
    return 0;
}