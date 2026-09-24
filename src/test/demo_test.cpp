#include "test.h"

#include <engine/shared/demo.h>
#include <engine/shared/network.h>
#include <engine/shared/protocol.h>
#include <engine/shared/snapshot.h>
#include <engine/storage.h>

#include <generated/protocol.h>

#include <game/version.h>

#include <gtest/gtest.h>

#include <cmath>
#include <memory>

class CIntraTickCheckListener : public CDemoPlayer::IListener
{
public:
	const CDemoPlayer *m_pDemoPlayer = nullptr;
	int m_NumSnapshots = 0;
	int m_NumInvalidIntraTicks = 0;

	void OnDemoPlayerSnapshot(void *pData, int Size) override
	{
		m_NumSnapshots++;
		if(!std::isfinite(m_pDemoPlayer->Info()->m_IntraTick))
		{
			m_NumInvalidIntraTicks++;
		}
	}

	void OnDemoPlayerMessage(void *pData, int Size) override {}
};

TEST(Demo, SeekIntraTickFinite)
{
	CTestInfo Info;
	Info.m_DeleteTestStorageFilesOnSuccess = true;
	std::unique_ptr<IStorage> pStorage = Info.CreateTestStorage();
	ASSERT_NE(pStorage, nullptr) << "Error creating test storage";

	CNetBase::Init();
	CSnapshotDelta SnapshotDelta;
	auto pRecorder = std::make_unique<CDemoRecorder>(&SnapshotDelta, true);
	// non-null so the recorder does not try to open the map file, no map data is recorded
	unsigned char MapData = 0;
	ASSERT_EQ(pRecorder->Start(pStorage.get(), nullptr, "test.demo", GAME_NETVERSION, "test", SHA256_DIGEST{}, 0, "client", 0, &MapData, nullptr, nullptr, nullptr), 0);
	auto pBuilder = std::make_unique<CSnapshotBuilder>();
	auto pSnapshot = std::make_unique<CSnapshotBuffer>();
	for(int Tick = 1; Tick <= 20 * SERVER_TICK_SPEED; Tick++)
	{
		pBuilder->Init();
		CNetObj_Flag Flag;
		Flag.m_X = Tick;
		Flag.m_Y = 0;
		Flag.m_Team = 0;
		EXPECT_TRUE(pBuilder->NewItem(NETOBJTYPE_FLAG, 0, &Flag, sizeof(Flag)));
		const int SnapshotSize = pBuilder->Finish(pSnapshot.get());
		pRecorder->RecordSnapshot(Tick, pSnapshot->AsSnapshot(), SnapshotSize);
	}
	ASSERT_EQ(pRecorder->Stop(IDemoRecorder::EStopMode::KEEP_FILE), 0);

	auto pPlayer = std::make_unique<CDemoPlayer>(&SnapshotDelta, &SnapshotDelta, false);
	CIntraTickCheckListener Listener;
	Listener.m_pDemoPlayer = pPlayer.get();
	pPlayer->SetListener(&Listener);
	ASSERT_EQ(pPlayer->Load(pStorage.get(), nullptr, "test.demo", IStorage::TYPE_SAVE), 0);
	pPlayer->Play();
	ASSERT_TRUE(pPlayer->IsPlaying());

	// seeking forwards past a keyframe and backwards both restart playback from a keyframe
	EXPECT_TRUE(pPlayer->SeekPercent(0.8f));
	EXPECT_TRUE(pPlayer->SeekPercent(0.2f));
	EXPECT_GT(Listener.m_NumSnapshots, 0);
	EXPECT_EQ(Listener.m_NumInvalidIntraTicks, 0);

	pPlayer->Stop();
}
