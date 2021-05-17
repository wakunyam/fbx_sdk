#include <fbxsdk.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>

using namespace std;

struct Joint
{
	string mName;
	int mParentIndex;
	FbxNode* mNode;
};

struct Skeleton
{
	vector<Joint> mJoints;
};

void ProcessSkeletonHierarchy(FbxNode*);
void ProcessSkeletonHierarchyRecursively(FbxNode*, int, int);
int FindJointIndexUsingName(const string&);
void ProcessJointAndAnimations(FbxNode*);
FbxAMatrix GetGeometry(FbxNode*);

Skeleton skeleton;

int main()
{
	const char* fileName = "skeletonanimwalk.fbx";

	FbxManager* fbxManager;
	FbxScene* fbxScene;

	fbxManager = FbxManager::Create();

	FbxIOSettings* fbxIOSettings = FbxIOSettings::Create(fbxManager, IOSROOT);
	fbxManager->SetIOSettings(fbxIOSettings);

	FbxImporter* fbxImporter = FbxImporter::Create(fbxManager, "");

	if (!fbxImporter->Initialize(fileName, -1, fbxManager->GetIOSettings())) {
		cout << "Call to FbxImporter::Initialize() failed." << endl;
		cout << "Error returned: " << fbxImporter->GetStatus().GetErrorString();
		exit(-1);
	}

	fbxScene = FbxScene::Create(fbxManager, "myScene");

	fbxImporter->Import(fbxScene);
	fbxImporter->Destroy();

	FbxNode* rootNode = fbxScene->GetRootNode();

	ProcessSkeletonHierarchy(rootNode);
	if (skeleton.mJoints.empty()) ProcessJointAndAnimations(rootNode);
}

void ProcessSkeletonHierarchy(FbxNode* inNode)
{
	for (int childIndex = 0; childIndex < inNode->GetChildCount(); ++childIndex) {
		FbxNode* childNode = inNode->GetChild(childIndex);
		if (childNode->GetNodeAttribute()->GetAttributeType() == FbxNodeAttribute::eSkeleton) {
			ProcessSkeletonHierarchyRecursively(childNode, 0, -1);
			break;
		}
	}
}

void ProcessSkeletonHierarchyRecursively(FbxNode* inNode, int myIndex, int parentIndex)
{
	if (!inNode) return;
	
	Joint currJoint;
	currJoint.mParentIndex = parentIndex;
	currJoint.mName = inNode->GetName();
	skeleton.mJoints.emplace_back(currJoint);
	
	for (int i = 0; i < inNode->GetChildCount(); ++i)
		ProcessSkeletonHierarchyRecursively(inNode->GetChild(i), skeleton.mJoints.size(),myIndex);
}

int FindJointIndexUsingName(const string& inJointName)
{
	for (int i = 0; i < skeleton.mJoints.size(); ++i) {
		if (skeleton.mJoints[i].mName == inJointName) return i;
	}

	return -1;
}

void ProcessJointAndAnimations(FbxNode* inNode)
{
	FbxMesh* currMesh = inNode->GetMesh();

	FbxAMatrix geometry = GetGeometry(inNode);

	for (int deformerIndex = 0; deformerIndex < currMesh->GetDeformerCount(); ++deformerIndex) {
		FbxSkin* currSkin = reinterpret_cast<FbxSkin*>(currMesh->GetDeformer(deformerIndex, FbxDeformer::eSkin));

		for (int clusterIndex = 0; clusterIndex < currSkin->GetClusterCount(); ++clusterIndex) {
			FbxCluster* currCluster = currSkin->GetCluster(clusterIndex);
			string currJointName = currCluster->GetLink()->GetName();
			int currJointIndex = FindJointIndexUsingName(currJointName);
			skeleton.mJoints[currJointIndex].mNode = currCluster->GetLink();

			FbxAMatrix transformLinkMatrix;
			currCluster->GetTransformLinkMatrix(transformLinkMatrix);

			
		}
	}
}

FbxAMatrix GetGeometry(FbxNode* inNode)
{
	const FbxVector4 lT = inNode->GetGeometricTranslation(FbxNode::eSourcePivot);
	const FbxVector4 lR = inNode->GetGeometricRotation(FbxNode::eSourcePivot);
	const FbxVector4 lS = inNode->GetGeometricScaling(FbxNode::eSourcePivot);

	return FbxAMatrix(lT, lR, lS);
}
