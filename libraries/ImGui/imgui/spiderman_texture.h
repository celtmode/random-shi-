#pragma once
#include <d3d11.h>

// Declares the Spider-Man texture so all .cpp files can use it
extern ID3D11ShaderResourceView* g_SpiderManTexture;

// Function to load the texture from the embedded spiderman_png data
bool LoadSpiderManTexture(ID3D11Device* device);

// Function to clean up
void ReleaseSpiderManTexture();
