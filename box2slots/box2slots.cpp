//***************************************************************************************
// BoxApp.cpp by Frank Luna (C) 2015 All Rights Reserved.
//
// Shows how to draw a box in Direct3D 12.
//
// Controls:
//   Hold the left mouse button down and move the mouse to rotate.
//   Hold the right mouse button down and move the mouse to zoom in and out.
//***************************************************************************************

#include "../Common/MathHelper.h"
#include "../Common/UploadBuffer.h "
#include "../Common/d3dApp.h"

#include <iostream>

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;

using std::cout;
using std::endl;

struct Vertex {
  XMFLOAT3 Pos;
  XMFLOAT4 Color;
};

struct vPosData {
  XMFLOAT3 Pos;
};

struct vColorData {
  XMFLOAT4 Color;
};

struct ObjectConstants {
  XMFLOAT4X4 WorldViewProj = MathHelper::Identity4x4();
  float time;
};

class BoxApp : public D3DApp {
public:
  BoxApp(HINSTANCE hInstance);
  BoxApp(const BoxApp &rhs) = delete;
  BoxApp &operator=(const BoxApp &rhs) = delete;
  ~BoxApp();

  virtual bool Initialize() override;

private:
  virtual void OnResize() override;
  virtual void Update(const GameTimer &gt) override;
  virtual void Draw(const GameTimer &gt) override;

  virtual void OnMouseDown(WPARAM btnState, int x, int y) override;
  virtual void OnMouseUp(WPARAM btnState, int x, int y) override;
  virtual void OnMouseMove(WPARAM btnState, int x, int y) override;

  void BuildDescriptorHeaps();
  void BuildConstantBuffers();
  void BuildRootSignature();
  void BuildShadersAndInputLayout();
  void BuildBoxGeometry();
  void BuildPSO();

private:
  ComPtr<ID3D12RootSignature> mRootSignature = nullptr;
  ComPtr<ID3D12DescriptorHeap> mCbvHeap = nullptr;

  std::unique_ptr<UploadBuffer<ObjectConstants>> mObjectCB = nullptr;

  std::unique_ptr<MeshGeometry> mBoxGeo = nullptr;
  std::unique_ptr<MeshGeometry> mCoordinateGeo = nullptr;

  ComPtr<ID3DBlob> mvsByteCode = nullptr;
  ComPtr<ID3DBlob> mpsByteCode = nullptr;

  std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;

  ComPtr<ID3D12PipelineState> mPSO = nullptr;

  XMFLOAT4X4 mWorld = MathHelper::Identity4x4();
  XMFLOAT4X4 mView = MathHelper::Identity4x4();
  XMFLOAT4X4 mProj = MathHelper::Identity4x4();

  float mTheta = 0.25f * XM_PI;
  float mPhi = XM_PIDIV4;
  float mRadius = 5.0f;

  POINT mLastMousePos;
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance, PSTR cmdLine,
                   int showCmd) {
  // Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
  _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

  XMVECTOR pos = XMVectorSet(-20, 45, 50, 0.0f);
  XMVECTOR target = XMVectorSet(10, 0, 30, 0.0f);
  XMVECTOR up = XMVectorSet(0.0, 1.0, 0.0, 0.0f);
  XMMATRIX v = XMMatrixLookAtLH(pos, target, up);

  std::cout << v << std::endl;
  std::cout << up << std::endl;

  std::wstring ss = L"�������\n";
  OutputDebugString(ss.c_str());

  try {
    BoxApp theApp(hInstance);
    if (!theApp.Initialize())
      return 0;

    return theApp.Run();
  } catch (DxException &e) {
    MessageBox(nullptr, e.ToString().c_str(), L"HR Failed", MB_OK);
    return 0;
  }
}

BoxApp::BoxApp(HINSTANCE hInstance) : D3DApp(hInstance) {}

BoxApp::~BoxApp() {}

bool BoxApp::Initialize() {
  if (!D3DApp::Initialize())
    return false;

  // Reset the command list to prep for initialization commands.
  ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

  BuildDescriptorHeaps();
  BuildConstantBuffers();
  BuildRootSignature();
  BuildShadersAndInputLayout();
  BuildBoxGeometry();
  BuildPSO();

  // Execute the initialization commands.
  ThrowIfFailed(mCommandList->Close());
  ID3D12CommandList *cmdsLists[] = {mCommandList.Get()};
  mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

  // Wait until initialization is complete.
  FlushCommandQueue();

  return true;
}

void BoxApp::OnResize() {
  D3DApp::OnResize();

  // The window resized, so update the aspect ratio and recompute the projection
  // matrix.
  XMMATRIX P = XMMatrixPerspectiveFovLH(0.25f * MathHelper::Pi, AspectRatio(),
                                        1.0f, 1000.0f);
  XMStoreFloat4x4(&mProj, P);
}

void BoxApp::Update(const GameTimer &gt) {
  // Convert Spherical to Cartesian coordinates.
  float x = mRadius * sinf(mPhi) * cosf(mTheta);
  float y = mRadius * sinf(mPhi) * sinf(mTheta);
  float z = mRadius * cosf(mPhi);

  std::cout << "x: " << x << " y: " << y << " z: " << z << std::endl;

  // Build the view matrix.
  XMVECTOR pos = XMVectorSet(x, y, z, 1.0f);
  XMVECTOR target = XMVectorZero();
  XMVECTOR up = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);

  XMMATRIX view = XMMatrixLookAtLH(pos, target, up);

  // cout << "view: " << endl << view << endl;
  XMStoreFloat4x4(&mView, view);

  XMMATRIX world = XMLoadFloat4x4(&mWorld);
  XMMATRIX proj = XMLoadFloat4x4(&mProj);
  XMMATRIX worldViewProj = world * view * proj;

  // Update the constant buffer with the latest worldViewProj matrix.
  ObjectConstants objConstants;
  XMStoreFloat4x4(&objConstants.WorldViewProj,
                  XMMatrixTranspose(worldViewProj));

  objConstants.time = gt.TotalTime();
  mObjectCB->CopyData(0, objConstants);
}

void BoxApp::Draw(const GameTimer &gt) {
  // Reuse the memory associated with command recording.
  // We can only reset when the associated command lists have finished execution
  // on the GPU.
  ThrowIfFailed(mDirectCmdListAlloc->Reset());

  // A command list can be reset after it has been added to the command queue
  // via ExecuteCommandList.
  // Reusing the command list reuses memory.
  ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), mPSO.Get()));

  mCommandList->RSSetViewports(1, &mScreenViewport);
  mCommandList->RSSetScissorRects(1, &mScissorRect);

  auto x6 = CD3DX12_RESOURCE_BARRIER::Transition(
      CurrentBackBuffer(), D3D12_RESOURCE_STATE_PRESENT,
      D3D12_RESOURCE_STATE_RENDER_TARGET);
  // Indicate a state transition on the resource usage.
  mCommandList->ResourceBarrier(1, &x6);

  // Clear the back buffer and depth buffer.
  mCommandList->ClearRenderTargetView(CurrentBackBufferView(),
                                      Colors::LightSteelBlue, 0, nullptr);
  mCommandList->ClearDepthStencilView(
      DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
      1.0f, 0, 0, nullptr);

  // Specify the buffers we are going to render to.
  auto x1 = DepthStencilView();
  auto x2 = CurrentBackBufferView();
  mCommandList->OMSetRenderTargets(1, &x2, true, &x1);

  ID3D12DescriptorHeap *descriptorHeaps[] = {mCbvHeap.Get()};
  mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

  mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

  auto x3 = mBoxGeo->VertexBufferView();
  mCommandList->IASetVertexBuffers(0, 1, &x3);
  mCommandList->IASetVertexBuffers(1, 1, &x3);
  auto x4 = mBoxGeo->IndexBufferView();
  mCommandList->IASetIndexBuffer(&x4);
  mCommandList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  mCommandList->SetGraphicsRootDescriptorTable(
      0, mCbvHeap->GetGPUDescriptorHandleForHeapStart());

  mCommandList->DrawIndexedInstanced(mBoxGeo->DrawArgs["box"].IndexCount, 1, 0,
                                     0, 0);

  auto x15 = mCoordinateGeo->VertexBufferView();
  mCommandList->IASetVertexBuffers(0, 1, &x15);
  auto x16 = mCoordinateGeo->IndexBufferView();
  mCommandList->IASetIndexBuffer(&x16);
  mCommandList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
  mCommandList->SetGraphicsRootDescriptorTable(
      0, mCbvHeap->GetGPUDescriptorHandleForHeapStart());

  mCommandList->DrawIndexedInstanced(
      mCoordinateGeo->DrawArgs["coordinate"].IndexCount, 1, 0, 0, 0);

  // Indicate a state transition on the resource usage.
  auto x7 = CD3DX12_RESOURCE_BARRIER::Transition(
      CurrentBackBuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET,
      D3D12_RESOURCE_STATE_PRESENT);
  mCommandList->ResourceBarrier(1, &x7);

  // Done recording commands.
  ThrowIfFailed(mCommandList->Close());

  // Add the command list to the queue for execution.
  ID3D12CommandList *cmdsLists[] = {mCommandList.Get()};
  mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

  // swap the back and front buffers
  ThrowIfFailed(mSwapChain->Present(0, 0));
  mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

  // Wait until frame commands are complete.  This waiting is inefficient and is
  // done for simplicity.  Later we will show how to organize our rendering code
  // so we do not have to wait per frame.
  FlushCommandQueue();
}

void BoxApp::OnMouseDown(WPARAM btnState, int x, int y) {
  mLastMousePos.x = x;
  mLastMousePos.y = y;

  SetCapture(mhMainWnd);
}

void BoxApp::OnMouseUp(WPARAM btnState, int x, int y) { ReleaseCapture(); }

void BoxApp::OnMouseMove(WPARAM btnState, int x, int y) {
  if ((btnState & MK_LBUTTON) != 0) {
    // Make each pixel correspond to a quarter of a degree.
    float dx =
        XMConvertToRadians(0.25f * static_cast<float>(x - mLastMousePos.x));
    float dy =
        XMConvertToRadians(0.25f * static_cast<float>(y - mLastMousePos.y));

    // Update angles based on input to orbit camera around box.
    mTheta += dx;
    mPhi += dy;

    cout << "mPhi: " << mPhi * 180 / 3.1415 << endl;

    // Restrict the angle mPhi.
    mPhi = MathHelper::Clamp(mPhi, 0 + 0.001f, MathHelper::Pi - 0.001f);
  } else if ((btnState & MK_RBUTTON) != 0) {
    // Make each pixel correspond to 0.005 unit in the scene.
    float dx = 0.005f * static_cast<float>(x - mLastMousePos.x);
    float dy = 0.005f * static_cast<float>(y - mLastMousePos.y);

    // Update the camera radius based on input.
    mRadius += dx - dy;

    // Restrict the radius.
    mRadius = MathHelper::Clamp(mRadius, 3.0f, 15.0f);
  }

  mLastMousePos.x = x;
  mLastMousePos.y = y;
}

void BoxApp::BuildDescriptorHeaps() {
  D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc;
  cbvHeapDesc.NumDescriptors = 1;
  cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
  cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
  cbvHeapDesc.NodeMask = 0;
  ThrowIfFailed(
      md3dDevice->CreateDescriptorHeap(&cbvHeapDesc, IID_PPV_ARGS(&mCbvHeap)));
}

void BoxApp::BuildConstantBuffers() {
  mObjectCB = std::make_unique<UploadBuffer<ObjectConstants>>(md3dDevice.Get(),
                                                              1, true);

  UINT objCBByteSize =
      d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));

  D3D12_GPU_VIRTUAL_ADDRESS cbAddress =
      mObjectCB->Resource()->GetGPUVirtualAddress();
  // Offset to the ith object constant buffer in the buffer.
  int boxCBufIndex = 0;
  cbAddress += boxCBufIndex * objCBByteSize;

  D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
  cbvDesc.BufferLocation = cbAddress;
  cbvDesc.SizeInBytes =
      d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));

  md3dDevice->CreateConstantBufferView(
      &cbvDesc, mCbvHeap->GetCPUDescriptorHandleForHeapStart());
}

void BoxApp::BuildRootSignature() {
  // Shader programs typically require resources as input (constant buffers,
  // textures, samplers).  The root signature defines the resources the shader
  // programs expect.  If we think of the shader programs as a function, and
  // the input resources as function parameters, then the root signature can be
  // thought of as defining the function signature.

  // Root parameter can be a table, root descriptor or root constants.
  CD3DX12_ROOT_PARAMETER slotRootParameter[1];

  // Create a single descriptor table of CBVs.
  CD3DX12_DESCRIPTOR_RANGE cbvTable;
  cbvTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
  slotRootParameter[0].InitAsDescriptorTable(1, &cbvTable);

  // A root signature is an array of root parameters.
  CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
      1, slotRootParameter, 0, nullptr,
      D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

  // create a root signature with a single slot which points to a descriptor
  // range consisting of a single constant buffer
  ComPtr<ID3DBlob> serializedRootSig = nullptr;
  ComPtr<ID3DBlob> errorBlob = nullptr;
  HRESULT hr = D3D12SerializeRootSignature(
      &rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
      serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

  if (errorBlob != nullptr) {
    ::OutputDebugStringA((char *)errorBlob->GetBufferPointer());
  }
  ThrowIfFailed(hr);

  ThrowIfFailed(md3dDevice->CreateRootSignature(
      0, serializedRootSig->GetBufferPointer(),
      serializedRootSig->GetBufferSize(), IID_PPV_ARGS(&mRootSignature)));
}

void BoxApp::BuildShadersAndInputLayout() {
  HRESULT hr = S_OK;

  mvsByteCode =
      d3dUtil::CompileShader(L"Shaders\\color.hlsl", nullptr, "VS", "vs_5_0");
  mpsByteCode =
      d3dUtil::CompileShader(L"Shaders\\color.hlsl", nullptr, "PS", "ps_5_0");

  mInputLayout = {{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
                   D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
                  {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 0,
                   D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}};
}

void BoxApp::BuildBoxGeometry() {
  std::array<Vertex, 8> vertices = {
      Vertex({XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT4(Colors::White)}),
      Vertex({XMFLOAT3(-1.0f, +1.0f, -1.0f), XMFLOAT4(Colors::Black)}),
      Vertex({XMFLOAT3(+1.0f, +1.0f, -1.0f), XMFLOAT4(Colors::Red)}),
      Vertex({XMFLOAT3(+1.0f, -1.0f, -1.0f), XMFLOAT4(Colors::Green)}),
      Vertex({XMFLOAT3(-1.0f, -1.0f, +1.0f), XMFLOAT4(Colors::Blue)}),
      Vertex({XMFLOAT3(-1.0f, +1.0f, +1.0f), XMFLOAT4(Colors::Yellow)}),
      Vertex({XMFLOAT3(+1.0f, +1.0f, +1.0f), XMFLOAT4(Colors::Cyan)}),
      Vertex({XMFLOAT3(+1.0f, -1.0f, +1.0f), XMFLOAT4(Colors::Magenta)})};

  std::array<Vertex, 6> vertices2 = {

      Vertex({XMFLOAT3(0.0f, 0.0f, 0.0f), XMFLOAT4(Colors::White)}),
      Vertex({XMFLOAT3(+2.0f, 0.0f, 0.0f), XMFLOAT4(Colors::Red)}),
      Vertex({XMFLOAT3(0.0f, 0.0f, 0.0f), XMFLOAT4(Colors::White)}),
      Vertex({XMFLOAT3(0.0f, 2.0f, 0.0f), XMFLOAT4(Colors::Green)}),
      Vertex({XMFLOAT3(0.0f, 0.0f, 0.0f), XMFLOAT4(Colors::White)}),
      Vertex({XMFLOAT3(0.0f, 0.0f, 2.0f), XMFLOAT4(Colors::Blue)})};

  std::array<std::uint16_t, 36> indices = {// front face
                                           0, 1, 2, 0, 2, 3,

                                           // back face
                                           4, 6, 5, 4, 7, 6,

                                           // left face
                                           4, 5, 1, 4, 1, 0,

                                           // right face
                                           3, 2, 6, 3, 6, 7,

                                           // top face
                                           1, 5, 6, 1, 6, 2,

                                           // bottom face
                                           4, 0, 3, 4, 3, 7

  };

  std::array<std::uint16_t, 9> indices2 = {0, 1, 0, 0, 3, 0, 0, 5, 0};

  const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
  const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

  const UINT vbByteSize2 = (UINT)vertices2.size() * sizeof(Vertex);
  const UINT ibByteSize2 = (UINT)indices2.size() * sizeof(std::uint16_t);

  mBoxGeo = std::make_unique<MeshGeometry>();
  mBoxGeo->Name = "boxGeo";

  mCoordinateGeo = std::make_unique<MeshGeometry>();
  mCoordinateGeo->Name = "coordinate";

  ThrowIfFailed(D3DCreateBlob(vbByteSize, &mBoxGeo->VertexBufferCPU));
  CopyMemory(mBoxGeo->VertexBufferCPU->GetBufferPointer(), vertices.data(),
             vbByteSize);

  ThrowIfFailed(D3DCreateBlob(ibByteSize, &mBoxGeo->IndexBufferCPU));
  CopyMemory(mBoxGeo->IndexBufferCPU->GetBufferPointer(), indices.data(),
             ibByteSize);

  ThrowIfFailed(D3DCreateBlob(vbByteSize2, &mCoordinateGeo->VertexBufferCPU));
  CopyMemory(mCoordinateGeo->VertexBufferCPU->GetBufferPointer(),
             vertices2.data(), vbByteSize2);

  ThrowIfFailed(D3DCreateBlob(ibByteSize2, &mCoordinateGeo->IndexBufferCPU));
  CopyMemory(mCoordinateGeo->IndexBufferCPU->GetBufferPointer(),
             indices2.data(), ibByteSize2);

  mBoxGeo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(
      md3dDevice.Get(), mCommandList.Get(), vertices.data(), vbByteSize,
      mBoxGeo->VertexBufferUploader);

  mBoxGeo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(
      md3dDevice.Get(), mCommandList.Get(), indices.data(), ibByteSize,
      mBoxGeo->IndexBufferUploader);

  mCoordinateGeo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(
      md3dDevice.Get(), mCommandList.Get(), vertices2.data(), vbByteSize2,
      mCoordinateGeo->VertexBufferUploader);

  mCoordinateGeo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(
      md3dDevice.Get(), mCommandList.Get(), indices2.data(), ibByteSize2,
      mCoordinateGeo->IndexBufferUploader);

  mBoxGeo->VertexByteStride = sizeof(Vertex);
  mBoxGeo->VertexBufferByteSize = vbByteSize;
  mBoxGeo->IndexFormat = DXGI_FORMAT_R16_UINT;
  mBoxGeo->IndexBufferByteSize = ibByteSize;

  mCoordinateGeo->VertexByteStride = sizeof(Vertex);
  mCoordinateGeo->VertexBufferByteSize = vbByteSize2;
  mCoordinateGeo->IndexFormat = DXGI_FORMAT_R16_UINT;
  mCoordinateGeo->IndexBufferByteSize = ibByteSize2;

  SubmeshGeometry submesh;
  submesh.IndexCount = (UINT)indices.size();
  submesh.StartIndexLocation = 0;
  submesh.BaseVertexLocation = 0;

  mBoxGeo->DrawArgs["box"] = submesh;

  SubmeshGeometry submesh2;
  submesh.IndexCount = (UINT)indices2.size();
  submesh.StartIndexLocation = 0;
  submesh.BaseVertexLocation = 0;

  mCoordinateGeo->DrawArgs["coordinate"] = submesh;
}

void BoxApp::BuildPSO() {
  D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc;
  ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
  psoDesc.InputLayout = {mInputLayout.data(), (UINT)mInputLayout.size()};
  psoDesc.pRootSignature = mRootSignature.Get();
  psoDesc.VS = {reinterpret_cast<BYTE *>(mvsByteCode->GetBufferPointer()),
                mvsByteCode->GetBufferSize()};
  psoDesc.PS = {reinterpret_cast<BYTE *>(mpsByteCode->GetBufferPointer()),
                mpsByteCode->GetBufferSize()};
  psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
  psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
  psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
  psoDesc.SampleMask = UINT_MAX;
  psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  psoDesc.NumRenderTargets = 1;
  psoDesc.RTVFormats[0] = mBackBufferFormat;
  psoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
  psoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
  psoDesc.DSVFormat = mDepthStencilFormat;
  ThrowIfFailed(
      md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSO)));
}
