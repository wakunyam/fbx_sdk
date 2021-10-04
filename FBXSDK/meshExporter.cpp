#include <fbxsdk.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>

using namespace std;

struct Vec3 {
	float x, y, z;
};

FbxMesh* gMesh = nullptr;
Vec3* vertices = nullptr;
unsigned int* indices = nullptr;
unsigned int numTriangles;
unsigned int numVertices;

void LoadNode(FbxNode* node)
{
	FbxNodeAttribute* nodeAttribute = node->GetNodeAttribute();
	if(nodeAttribute)
		if (nodeAttribute->GetAttributeType() == FbxNodeAttribute::eMesh) {
			gMesh = node->GetMesh();
		}

	const int childCount = node->GetChildCount();
	for (unsigned int i = 0; i < childCount; ++i) 
		LoadNode(node->GetChild(i));
}

void ProcessControlPoints(FbxMesh* mesh)
{
	numVertices = mesh->GetControlPointsCount();
	vertices = new Vec3[numVertices];

	for (unsigned int i = 0; i < numVertices; ++i) {
		vertices[i].x = -mesh->GetControlPointAt(i).mData[0];
		vertices[i].y = mesh->GetControlPointAt(i).mData[1];
		vertices[i].z = mesh->GetControlPointAt(i).mData[2];
	}
}

void WriteMeshData(ostream& inStream)
{
	inStream << "{" << endl;
	inStream << "\t\"vertices\": {" << endl;
	inStream << "\t\t\"numVertices\": " << numVertices << "," << endl;
	inStream << "\t\t\"vertex\": [" << endl;
	for (unsigned int i = 0; i < numVertices; ++i) {
		inStream << "\t\t\t{" << endl;
		inStream << "\t\t\t\t\"x\": " << vertices[i].x << ",\t\"y\": " << vertices[i].y << ",\t\"z\": " << vertices[i].z << endl;
		if (i < numVertices - 1)
			inStream << "\t\t\t}," << endl;
		else
			inStream << "\t\t\t}" << endl;
	}
	inStream << "\t\t]" << endl;
	inStream << "\t}," << endl;
	inStream << "\t\"indices\": {" << endl;
	inStream << "\t\t\"numTriangles\": " << numTriangles << "," << endl;
	inStream << "\t\t\"index\": [" << endl;
	for (unsigned int i = 0; i < numTriangles * 3; ++i) {
		if (i % 3 == 0)
			inStream << "\t\t\t";
		inStream << indices[i]; 
		if (i < numTriangles * 3 - 1)
			inStream << ",\t";
		if (i % 3 == 2)
			inStream << endl;
	}
	inStream << "\t\t]" << endl;
	inStream << "\t}" << endl;
	inStream << "}";
}

int main()
{
	string infolder = "fbx\\";
	string fileName = "stair2";
	string fn = infolder + fileName + ".fbx";

	FbxManager* fbxManager;
	FbxScene* fbxScene;

	fbxManager = FbxManager::Create();

	FbxIOSettings* fbxIOSettings = FbxIOSettings::Create(fbxManager, IOSROOT);
	fbxManager->SetIOSettings(fbxIOSettings);

	FbxImporter* fbxImporter = FbxImporter::Create(fbxManager, "");

	if (!fbxImporter->Initialize(fn.c_str(), -1, fbxManager->GetIOSettings())) {
		cout << "Call to FbxImporter::Initialize() failed." << endl;
		cout << "Error returned: " << fbxImporter->GetStatus().GetErrorString();
		exit(-1);
	}

	fbxScene = FbxScene::Create(fbxManager, "myScene");

	fbxImporter->Import(fbxScene);
	fbxImporter->Destroy();

	FbxNode* rootNode = fbxScene->GetRootNode();

	LoadNode(rootNode);
	ProcessControlPoints(gMesh);
	numTriangles = gMesh->GetPolygonCount();
	indices = new unsigned int[numTriangles * 3];
	int nbIndex = 0;
	for (unsigned int i = 0; i < numTriangles; ++i) {
		//for (unsigned int j = 0; j < 3; ++j) {
		for (int j = 2; j >= 0; --j) {
			int controlPointIndex = gMesh->GetPolygonVertex(i, j);

			indices[nbIndex++] = controlPointIndex;
		}
	}

	string outfolder = "meshData\\";
	fn = outfolder + fileName + "MeshData.json";
	ofstream out(fn, ios::binary);

	WriteMeshData(out);
}