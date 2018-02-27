#include <sstream>
#include <iomanip>
#include "ProcMem.h"
#include "Overlay/Overlay.h"
#include "Overlay/D3D9Overlay.h"
#include "Overlay/Color.h"
#include "stdafx.h"
#include "tinyxml2.h"
#include <map>
//#include <boost/any.hpp>

#pragma region Offsets
#define dwBasePlayerPTR 0x00A33504
#define dwViewMatrix 0x04A40844
#define dwEntityList 0x04A4ECA4
#define dwEntityOffset 0x10
#define dwHealthOffset 0xFC
#define dwArmorOffset 0xA9E8
#define dwTeamOffset 0xF0
#define dwDormantOffset 0xE9
#define dwPositionOffset 0x134
#define dwBoneMatrixPTROffset 0x2698
#define dwBoneOffset 0x30
#define dwBoneXOffset 0x0C
#define dwBoneZOffset 0x2C
#define dwBoneYOffset 0x1C
#define dwViewAngleOffset 0x4D0C
#define dwViewOffset 0x104
#define dwOnGround 0x100 //257 on ground 256 air
#define dwCrosshairID 0xAA44
#define dwGlowObjBase 0x04F64BCC
#pragma endregion

#pragma region Values
const std::vector<std::vector<int>> skeleton = {
	//Neck to left
	{ 5,36,35,10,33,11 },

	// Neck to right
	{ 5,66,65,40,63,41 },

	// Back
	{ 5,4,3,2,1,0 },

	// Leg Left
	{ 0,68,72,73,69,70,71 },

	// Leg Right
	{ 0,74,78,79,75,76,77 }
};
const std::vector<int> neededBones = { 6,5,36,35,10,33,11,66,65,40,63,41,4,3,2,1,0,68,72,73,69,70,71,74,78,79,75,76,77 };
int triggerDelay = 80;// in milliseconds
int aimbotSmooth = 5;// amount of jumps
Overlay::Color boxColor(0, 0, 0);
Overlay::Color skeletonColor(170, 170, 170);
Overlay::Color healthBarFGColor(255, 0, 0);
Overlay::Color healthBarBGColor(100, 0, 0);
Overlay::Color armorBarFGColor(100, 100, 100);
Overlay::Color armorBarBGColor(50, 50, 50);
Overlay::Color distanceTextColor(0, 0, 0);
Overlay::Color headColor(255, 0, 0);
Overlay::Color allyGlowColor(0, 0, 255);
Overlay::Color enemyGlowColor(255, 0, 0);
Overlay::Color weaponGlowColor(255, 255, 255);
bool triggerBotEnabled = true;
bool aimBotEnabled = false;
bool espEnabled = true;
std::map<std::string, boost::any >configValues = { { "boxColor",&boxColor },
{ "skeletonColor",&skeletonColor } ,
{ "healthBarFGColor",&healthBarFGColor } ,
{ "healthBarBGColor",&healthBarBGColor } ,
{ "armorBarFGColor",&armorBarFGColor } ,
{ "armorBarBGColor",&armorBarBGColor } ,
{ "distanceTextColor",&distanceTextColor } ,
{ "headColor",&headColor } ,
{ "allyGlowColor",&allyGlowColor } ,
{ "enemyGlowColor",&enemyGlowColor } ,
{ "weaponGlowColor",&weaponGlowColor },
{ "triggerDelay",&triggerDelay } ,
{ "aimbotSmooth",&aimbotSmooth } };
#pragma endregion

#pragma region Globals
DWORD client, engine;
Memory Mem;
#pragma endregion

#pragma region Structures
struct Angles {
	float yaw;
	float pitch;
};
typedef float ViewMatrix[4][4];
ViewMatrix viewMatrix;
struct myRECT : RECT {
	int width;
	int height;
	void calcSize() {
		width = (int)(right - left);
		height = (int)(bottom - top);
	}
}wRECT;
struct Entity {
	int health, armor;
	bool dormant;
	Vec3 position;
	Vec3 bones[81];
	Vec3 *headPos = &bones[6];
	int team;
	int id;
	DWORD entityBase;
	DWORD boneBase;
	bool init(int i) {
		id = i;
		entityBase = Mem.Read<DWORD>(client + dwEntityList + dwEntityOffset*id);
		return entityBase != 0;
	}
	void getInfo() {
		dormant = Mem.Read<int>(entityBase + dwDormantOffset);
		if (!dormant)
		{
			position = Mem.Read<Vec3>(entityBase + dwPositionOffset);
			health = Mem.Read<int>(entityBase + dwHealthOffset);
			armor = Mem.Read<int>(entityBase + dwArmorOffset);
			team = Mem.Read<int>(entityBase + dwTeamOffset);
			boneBase = Mem.Read<DWORD>(entityBase + dwBoneMatrixPTROffset);
			for (int iJoint : neededBones)
			{
				bones[iJoint].x = Mem.Read<float>(boneBase + dwBoneOffset*iJoint + dwBoneXOffset);
				bones[iJoint].y = Mem.Read<float>(boneBase + dwBoneOffset*iJoint + dwBoneYOffset);
				bones[iJoint].z = Mem.Read<float>(boneBase + dwBoneOffset*iJoint + dwBoneZOffset);
			}
		}
	}
}MyPlayer, entity;
struct GlowObject
{
	DWORD pEntity;
	float r;
	float g;
	float b;
	float a;
	uint8_t unk1[16];
	bool m_bRenderWhenOccluded;
	bool m_bRenderWhenUnoccluded;
	bool m_bFullBloom;
	uint8_t unk2[14];
};
#pragma endregion

#pragma region Functions
inline bool file_exists(const std::string name) {
	struct stat buffer;
	return (stat(name.c_str(), &buffer) == 0);
}
template<class T>
T readXML(tinyxml2::XMLElement *element)
{
	T to;
	std::stringstream ss;
	ss << element->GetText();
	ss >> to;
	return to;
}
template<>
Overlay::Color readXML(tinyxml2::XMLElement *element)
{
	return Overlay::Color(readXML<int>(element->FirstChildElement("r")), readXML<int>(element->FirstChildElement("g")), readXML<int>(element->FirstChildElement("b")), readXML<int>(element->FirstChildElement("a")));
}
void saveConfiguration(std::string fileName) {
	tinyxml2::XMLDocument conf;
	tinyxml2::XMLNode * pRoot = conf.NewElement("Configuration");
	conf.InsertEndChild(pRoot);
	for (auto data : configValues)
	{
		tinyxml2::XMLElement *tag = conf.NewElement(data.first.c_str());
		std::string className = data.second.type().name();
		if (className == "class Overlay::Color *")
		{
			Overlay::Color *color = boost::any_cast<Overlay::Color *>(data.second);
			tinyxml2::XMLElement *r = conf.NewElement("r");
			r->SetText(color->r());
			tag->InsertEndChild(r);

			tinyxml2::XMLElement *g = conf.NewElement("g");
			g->SetText(color->g());
			tag->InsertEndChild(g);

			tinyxml2::XMLElement *b = conf.NewElement("b");
			b->SetText(color->b());
			tag->InsertEndChild(b);

			tinyxml2::XMLElement *a = conf.NewElement("a");
			a->SetText(color->a());
			tag->InsertEndChild(a);
		}
		else if (className == "int *")
			tag->SetText(*boost::any_cast<int *>(data.second));
		else if (className == "float *")
			tag->SetText(*boost::any_cast<float *>(data.second));
		else if (className == "double *")
			tag->SetText(*boost::any_cast<double *>(data.second));
		else if (className == "bool *")
			tag->SetText(*boost::any_cast<bool *>(data.second));
		pRoot->InsertEndChild(tag);
	}
	conf.SaveFile(fileName.c_str());
}
void loadConfiguration(std::string fileName) {
	tinyxml2::XMLDocument conf;
	conf.LoadFile(fileName.c_str());
	tinyxml2::XMLNode * pRoot = conf.FirstChild();
	for (auto data : configValues)
	{
		tinyxml2::XMLElement *tag = pRoot->FirstChildElement(data.first.c_str());
		if (tag == NULL) {
			MessageBox(0, "Crash", "Corrupt configuration file", MB_OK);
			exit(2);
		}
		std::string className = data.second.type().name();
		if (className == "class Overlay::Color *")
			*boost::any_cast<Overlay::Color *>(data.second) = readXML<Overlay::Color>(tag);
		else if (className == "int *")
			*boost::any_cast<int *>(data.second) = readXML<int>(tag);
		else if (className == "float *")
			*boost::any_cast<int *>(data.second) = readXML<float>(tag);
		else if (className == "double *")
			*boost::any_cast<int *>(data.second) = readXML<double>(tag);
		else if (className == "bool *")
			*boost::any_cast<int *>(data.second) = readXML<bool>(tag);
	}
}
inline float get3Ddistance(Vec3 *fromCoords, Vec3 *toCoords)
{
	return hypot(hypot(fromCoords->x - toCoords->x, fromCoords->y - toCoords->y), fromCoords->z - toCoords->z);
}
void calculateAngle(Vec3 *fromCoords, Vec3 *toCoords, Angles *angles)
{
	float radToDeg = 180 / M_PI;
	double delta[3] = { (fromCoords->x - toCoords->x), (fromCoords->y - toCoords->y), (fromCoords->z - toCoords->z) };
	double hyp = get3Ddistance(fromCoords, toCoords);
	angles->yaw = (float)(asinf(delta[2] / hyp) * radToDeg);
	angles->pitch = (float)(atanf(delta[1] / delta[0]) * radToDeg);
	if (delta[0] >= 0.0) { angles->pitch += 180.0f; }
}
bool worldToScreenSpace(Vec3 *targetCoords, Vec2 *ScreenSpaceCoords)
{
	float w = viewMatrix[3][0] * targetCoords->x + viewMatrix[3][1] * targetCoords->y + viewMatrix[3][2] * targetCoords->z + viewMatrix[3][3];
	if (w < 0.01f)
	{
		ScreenSpaceCoords = NULL;
		return false;
	}

	ScreenSpaceCoords->x = viewMatrix[0][0] * targetCoords->x + viewMatrix[0][1] * targetCoords->y + viewMatrix[0][2] * targetCoords->z + viewMatrix[0][3];
	ScreenSpaceCoords->y = viewMatrix[1][0] * targetCoords->x + viewMatrix[1][1] * targetCoords->y + viewMatrix[1][2] * targetCoords->z + viewMatrix[1][3];

	float invw = 1.0f / w;
	ScreenSpaceCoords->x *= invw;
	ScreenSpaceCoords->y *= invw;

	float x = wRECT.width / 2.0f;
	float y = wRECT.height / 2.0f;

	x += 0.5 * ScreenSpaceCoords->x * wRECT.width + 0.5;
	y -= 0.5 * ScreenSpaceCoords->y * wRECT.height + 0.5;

	ScreenSpaceCoords->x = x + wRECT.left;
	ScreenSpaceCoords->y = y + wRECT.top;

	return true;
}
#pragma endregion

int WinMain(HINSTANCE hInstance, HINSTANCE hPsrevInstance, LPTSTR lpCmdLine, int cmdShow)
{
#pragma region initialization
#pragma region load Configuration file
	if (file_exists("conf.xml"))
		loadConfiguration("conf.xml");
	else
		saveConfiguration("conf.xml");
#pragma endregion
	Mem.ProcAttach("csgo.exe");
	GetWindowRect(FindWindow(0, "Counter-Strike: Global Offensive"), &wRECT);
	wRECT.calcSize();
	client = Mem.GetModule("client.dll");
	engine = Mem.GetModule("engine.dll");
	MyPlayer.init(0);
	auto &pOverlay = Overlay::IOverlay::GetInstance();
	pOverlay = std::make_shared< Overlay::CD3D9Overlay >();
	pOverlay->GetSurface()->PrepareFont("Default", "Arial", 14, FW_BOLD, 0, 0, FALSE);
#pragma endregion
	if (pOverlay->Create("Counter-Strike: Global Offensive"))
	{
		auto printBoxes = [](Overlay::IOverlay *pOverlay, std::shared_ptr< Overlay::ISurface > pSurface)
		{



			MyPlayer.init(0);
			MyPlayer.getInfo();

#pragma region glow objects
			DWORD glowObjBase = Mem.Read<DWORD>(client + dwGlowObjBase);
			int objCount = Mem.Read<DWORD>(client + dwGlowObjBase + 0xC);
			for (size_t i = 0; i < objCount; i++)
			{

				GlowObject glowObj = Mem.Read<GlowObject>(glowObjBase + sizeof(GlowObject)*i);
				int team = Mem.Read<int>(glowObj.pEntity + dwTeamOffset);
				if (team == MyPlayer.team)
				{
					glowObj.r = allyGlowColor.r() / 255.0f;
					glowObj.g = allyGlowColor.g() / 255.0f;
					glowObj.b = allyGlowColor.b() / 255.0f;
					glowObj.a = allyGlowColor.a() / 255.0f;

				}
				else if (team == 2 || team == 3) {

					glowObj.r = enemyGlowColor.r() / 255.0f;
					glowObj.g = enemyGlowColor.g() / 255.0f;
					glowObj.b = enemyGlowColor.b() / 255.0f;
					glowObj.a = enemyGlowColor.a() / 255.0f;
				}

				glowObj.m_bRenderWhenOccluded = true;
				glowObj.m_bRenderWhenUnoccluded = false;
				Mem.Write<GlowObject>(glowObjBase + sizeof(GlowObject)*i, glowObj);
			}
#pragma endregion

			int crosshairID = Mem.Read<int>(MyPlayer.entityBase + dwCrosshairID) - 1;
#pragma region BHOP
			/*if (GetAsyncKeyState(VK_SPACE) & 0x8000 && Mem.Read<int>(MyPlayer.entityBase + dwOnGround) == 257)
		{
			keybd_event(0x39, 0x09, 0, 0);
			keybd_event(0x39, 0x09, KEYEVENTF_KEYUP, 0);
		}*/
#pragma endregion

			Vec3 closestHead;
			closestHead.x = NULL;
			ReadProcessMemory(Mem.HProcess, (LPVOID)(client + dwViewMatrix), &viewMatrix, sizeof(ViewMatrix), NULL);
			Vec3 myPlayerHead;
			D3DXVec3Add(&myPlayerHead, &MyPlayer.position, &Mem.Read<Vec3>(MyPlayer.entityBase + dwViewOffset));
			for (size_t i = 1; entity.init(i); i++)
			{

				entity.getInfo();
				Vec2 originTo, headTo;
				if (!entity.dormant&&entity.health > 0 && entity.team != MyPlayer.team && worldToScreenSpace(&entity.position, &originTo) && worldToScreenSpace(entity.headPos, &headTo))
				{
#pragma region trigger bot
					if (triggerBotEnabled)
						if (i == crosshairID)
						{
							mouse_event(MOUSEEVENTF_LEFTDOWN, NULL, NULL, NULL, NULL);
							mouse_event(MOUSEEVENTF_LEFTUP, NULL, NULL, NULL, NULL);
						}
#pragma endregion

					Vec3 topLeft;

#pragma region print joints
					/*for (int i = 0; i< sizeof(entity.bones) / sizeof(Vec3); i++)
							if (worldToScreenSpace(&entity.bones[i], &boneTo)) {
								float distance = get3Ddistance(&MyPlayer.position, &entity.position);
								int width = 500 / distance;
								int height = 500 / distance;
								boneTo.x = boneTo.x - (width / 2);
								boneTo.y = boneTo.y - height;
								pSurface->Rect(boneTo.x, boneTo.y, width, height, Overlay::Color(0, 0, 0));
								char text[20];
								sprintf(text, "%d", i);
								pSurface->String(boneTo.x, boneTo.y + height, "Default", Overlay::Color(0, 0, 255), text);
							}*/
#pragma endregion
					float distance = get3Ddistance(&myPlayerHead, entity.headPos);
#pragma region print skeleton
					for (auto bone : skeleton)
					{
						Vec2 bone1, bone2;
						bone1.x = NULL;
						for (auto iJoint : bone)
						{
							if (worldToScreenSpace(&entity.bones[iJoint], &bone2) && bone1.x != NULL)
								pSurface->Line(bone1.x, bone1.y, bone2.x, bone2.y, skeletonColor, 4.0f * 200 / distance);
							bone1 = bone2;
						}
					}
#pragma endregion
					float closestDistance = get3Ddistance(&myPlayerHead, &closestHead);
					if (closestHead.x == NULL || distance < closestDistance)
						closestHead = *entity.headPos;

					int height = originTo.y - headTo.y;
					int width = height / 2;
					topLeft.x = originTo.x - (width / 2);
					topLeft.y = originTo.y - height;
					int boxThickness = 5 * 400 / distance;
					if (boxThickness < 1)
						boxThickness = 1;

#pragma region print box
					//Box
					pSurface->BorderBox(topLeft.x, topLeft.y, width, height, boxThickness, boxColor);

					//Health
					pSurface->Rect(topLeft.x - width / 15, topLeft.y, width / 15, height + boxThickness, healthBarBGColor);
					pSurface->Rect(topLeft.x - width / 15, topLeft.y + (100 - entity.health) / 100.0f*(height + boxThickness), width / 15, entity.health / 100.0f*(height + boxThickness), healthBarFGColor);

					//Armor
					pSurface->Rect(topLeft.x - (width / 15) * 2, topLeft.y, width / 15, height + boxThickness, armorBarBGColor);
					pSurface->Rect(topLeft.x - (width / 15) * 2, topLeft.y + (100 - entity.armor) / 100.0f*(height + boxThickness), width / 15, entity.armor / 100.0f*(height + boxThickness), armorBarFGColor);

					//Head
					pSurface->Circle(headTo.x, headTo.y, 50 * 100 / distance, 50, 10.0f * 100 / distance, headColor);

					//Distance
					std::stringstream ss;
					ss << std::fixed << std::setprecision(2) << distance;
					pSurface->String(originTo.x - width / 2, originTo.y + boxThickness, "Default", distanceTextColor, ss.str().c_str());
#pragma endregion
				}
			}

#pragma region aimbot
			if (aimBotEnabled)
				if (closestHead.x != NULL) {
					Angles fromAngles, toAngles, angleDif;
					calculateAngle(&myPlayerHead, &closestHead, &toAngles);
					fromAngles = Mem.Read<Angles>(Mem.Read<DWORD>(engine + 0x005B92A4) + dwViewAngleOffset);
					angleDif.yaw = toAngles.yaw - fromAngles.yaw;
					angleDif.pitch = toAngles.pitch - fromAngles.pitch;
					for (size_t i = 0; i < aimbotSmooth; i++)
					{
						Angles smooth = fromAngles;
						smooth.yaw += angleDif.yaw / aimbotSmooth * i;
						smooth.pitch += angleDif.pitch / aimbotSmooth * i;
						Mem.Write(Mem.Read<DWORD>(engine + 0x005B92A4) + dwViewAngleOffset, smooth);
					}
					mouse_event(MOUSEEVENTF_LEFTDOWN, NULL, NULL, NULL, NULL);
					mouse_event(MOUSEEVENTF_LEFTUP, NULL, NULL, NULL, NULL);
				}
#pragma endregion
		};


		pOverlay->AddToCallback(printBoxes);
		while (pOverlay->Render())
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	pOverlay->Shutdown();

	return 0;
}