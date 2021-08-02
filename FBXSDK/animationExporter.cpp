#include <fbxsdk.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>

using namespace std;

struct KeyFrame
{
	FbxLongLong mFrameNum;
	FbxAMatrix mGlobalTransform;
	KeyFrame* mNext;

	KeyFrame() {
		this->mNext = nullptr;
	}
};

struct Joint
{
	string mName;
	int mParentIndex;
	FbxNode* mNode;
	FbxAMatrix mGlobalBindposeInverse;
	KeyFrame* mAnimation;

	Joint() {
		this->mAnimation = nullptr;
	}
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
void WriteMatrix(ostream&, const FbxMatrix&, int);
void WriteAnimationToStream(ostream&);

Skeleton skeleton;
FbxScene* fbxScene;
string animationName;
FbxLongLong animationLength;

int main()
{
	string fileName = "skeletonanim";
	string fn = fileName + ".fbx";

	FbxManager* fbxManager;

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

	ProcessSkeletonHierarchy(rootNode);
	if (!skeleton.mJoints.empty()) ProcessJointAndAnimations(rootNode);

	fileName += "animData.json";
	ofstream out(fileName, ios::binary);

	WriteAnimationToStream(out);
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
		ProcessSkeletonHierarchyRecursively(inNode->GetChild(i), skeleton.mJoints.size(), myIndex);
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
	if (inNode->GetNodeAttribute()) {
		if (inNode->GetNodeAttribute()->GetAttributeType() == FbxNodeAttribute::eMesh) {
			FbxMesh* currMesh = inNode->GetMesh();

			FbxAMatrix geometry = GetGeometry(inNode);

			for (int deformerIndex = 0; deformerIndex < currMesh->GetDeformerCount(); ++deformerIndex) {
				FbxSkin* currSkin = reinterpret_cast<FbxSkin*>(currMesh->GetDeformer(deformerIndex, FbxDeformer::eSkin));

				for (int clusterIndex = 0; clusterIndex < currSkin->GetClusterCount(); ++clusterIndex) {
					FbxCluster* currCluster = currSkin->GetCluster(clusterIndex);
					string currJointName = currCluster->GetLink()->GetName();
					int currJointIndex = FindJointIndexUsingName(currJointName);
					skeleton.mJoints[currJointIndex].mNode = currCluster->GetLink();

					FbxAMatrix transformMatrix;
					FbxAMatrix transformLinkMatrix;
					FbxAMatrix globalBindposeInverseMatrix;

					currCluster->GetTransformMatrix(transformMatrix);
					currCluster->GetTransformLinkMatrix(transformLinkMatrix);
					globalBindposeInverseMatrix = transformLinkMatrix.Inverse() * transformMatrix * geometry;
					// bind pose는 모델에 뼈를 심었을때 맨 처음 포즈, 모델링이 뼈에 영향을 받지 않은 모습

					skeleton.mJoints[currJointIndex].mGlobalBindposeInverse = globalBindposeInverseMatrix;

					FbxAnimStack* currAnimStack = fbxScene->GetSrcObject<FbxAnimStack>(0);
					FbxString animStackName = currAnimStack->GetName();
					animationName = animStackName.Buffer();
					FbxTakeInfo* takeInfo = fbxScene->GetTakeInfo(animStackName);
					FbxTime start = takeInfo->mLocalTimeSpan.GetStart();
					FbxTime end = takeInfo->mLocalTimeSpan.GetStop();
					animationLength = end.GetFrameCount() - start.GetFrameCount() + 1;
					KeyFrame** currAnim = &skeleton.mJoints[currJointIndex].mAnimation;
					for (FbxLongLong i = start.GetFrameCount(); i <= end.GetFrameCount(); ++i) {
						FbxTime currTime;
						currTime.SetFrame(i);
						*currAnim = new KeyFrame();
						(*currAnim)->mFrameNum = i;
						FbxAMatrix currentTransformOffset = inNode->EvaluateGlobalTransform(currTime) * geometry;
						(*currAnim)->mGlobalTransform = currentTransformOffset.Inverse() * currCluster->GetLink()->EvaluateGlobalTransform(currTime);
						currAnim = &((*currAnim)->mNext);
					}
				}
			}
		}
	}

	for (int i = 0; i < inNode->GetChildCount(); ++i)
		ProcessJointAndAnimations(inNode->GetChild(i));
}

FbxAMatrix GetGeometry(FbxNode* inNode)
{
	const FbxVector4 lT = inNode->GetGeometricTranslation(FbxNode::eSourcePivot);
	const FbxVector4 lR = inNode->GetGeometricRotation(FbxNode::eSourcePivot);
	const FbxVector4 lS = inNode->GetGeometricScaling(FbxNode::eSourcePivot);

	return FbxAMatrix(lT, lR, lS);
}

void WriteMatrix(ostream& inStream, const FbxMatrix& inMatrix, int tabCount)
{
	for (int i = 0; i < tabCount; ++i) inStream << "\t";
	inStream << "\"mat\": [" << endl;
	for (int i = 0; i < tabCount; ++i) inStream << "\t";
	inStream << "\t" << static_cast<float>(inMatrix.Get(0, 0)) << ", " << static_cast<float>(inMatrix.Get(0, 1)) << ", " << static_cast<float>(inMatrix.Get(0, 2)) << ", " << static_cast<float>(inMatrix.Get(0, 3)) << ", "
		<< static_cast<float>(inMatrix.Get(1, 0)) << ", " << static_cast<float>(inMatrix.Get(1, 1)) << ", " << static_cast<float>(inMatrix.Get(1, 2)) << ", " << static_cast<float>(inMatrix.Get(1, 3)) << ", "
		<< static_cast<float>(inMatrix.Get(2, 0)) << ", " << static_cast<float>(inMatrix.Get(2, 1)) << ", " << static_cast<float>(inMatrix.Get(2, 2)) << ", " << static_cast<float>(inMatrix.Get(2, 3)) << ", "
		<< static_cast<float>(inMatrix.Get(3, 0)) << ", " << static_cast<float>(inMatrix.Get(3, 1)) << ", " << static_cast<float>(inMatrix.Get(3, 2)) << ", " << static_cast<float>(inMatrix.Get(3, 3)) << endl;
	for (int i = 0; i < tabCount; ++i) inStream << "\t";
	inStream << "]" << endl;
}

void WriteAnimationToStream(ostream& inStream)
{
	inStream << "{" << endl;
	inStream << "\t\"skeleton\": {" << endl;
	inStream << "\t\t\"count\": " << skeleton.mJoints.size() << "," << endl;
	inStream << "\t\t\"joint\": [" << endl;
	for (int i = 0; i < skeleton.mJoints.size(); ++i) {
		inStream << "\t\t\t{" << endl;
		inStream << "\t\t\t\t\"id\": " << i << "," << endl;
		inStream << "\t\t\t\t\"name\": \"" << skeleton.mJoints[i].mName << "\"," << endl;
		inStream << "\t\t\t\t\"parent\": " << skeleton.mJoints[i].mParentIndex << "," << endl;
		FbxMatrix out = skeleton.mJoints[i].mGlobalBindposeInverse;
		WriteMatrix(inStream, out.Transpose(), 4);
		inStream << "\t\t\t}," << endl;
	}
	inStream << "\t\t]" << endl;
	inStream << "\t}," << endl;
	inStream << "\t\"animation\": [" << endl;
	inStream << "\t\t{" << endl;
	inStream << "\t\t\t\"name\": \"" << animationName << "\"," << endl;
	inStream << "\t\t\t\"animationLength\": " << animationLength << "," << endl;
	inStream << "\t\t\t\"track\": [" << endl;
	for (int i = 0; i < skeleton.mJoints.size(); ++i) {
		inStream << "\t\t\t\t{" << endl;
		inStream << "\t\t\t\t\t\"id\": " << i << "," << endl;
		inStream << "\t\t\t\t\t\"name\": \"" << skeleton.mJoints[i].mName << "\"," << endl;
		KeyFrame* walker = skeleton.mJoints[i].mAnimation;
		inStream << "\t\t\t\t\t\"frame\": [" << endl;
		while (walker) {
			inStream << "\t\t\t\t\t\t{" << endl;
			inStream << "\t\t\t\t\t\t\t\"num\": " << walker->mFrameNum << "," << endl;
			FbxMatrix out = walker->mGlobalTransform;
			WriteMatrix(inStream, out.Transpose(), 7);
			inStream << "\t\t\t\t\t\t}," << endl;
			walker = walker->mNext;
		}
		inStream << "\t\t\t\t\t]" << endl;
		inStream << "\t\t\t\t\t}," << endl;
	}
	inStream << "\t\t\t]" << endl;
	inStream << "\t\t}" << endl;
	inStream << "\t]" << endl;
	inStream << "}";
}
