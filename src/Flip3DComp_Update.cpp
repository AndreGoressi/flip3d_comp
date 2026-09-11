// ============================================================================
// Flip3DComp_Update.cpp — Per-frame update, camera, card transforms, scroll
// ============================================================================
//
// Carousel uses fractional smooth scroll (m_scrollPos) for browse.
// Each card's m_displaySlot is the single render coordinate during exit;
// browse computes it on the fly (CaptureCarouselVisualSlot), exit updates it
// in UpdateVisualSlots once per frame.
//
// ============================================================================
#include "Flip3DComp.h"

#include <algorithm>
#include <cmath>
#include <vector>

// ============================================================================
// Flip3DComp::EnterProgress
// ============================================================================
float Flip3DComp::EnterProgress() const
{
    return std::max(std::min(m_animEnter.Value(), 1.0f), 0.0f);
}

// ============================================================================
// Flip3DComp::ReplayEnterAnimation
// ============================================================================
void Flip3DComp::ReplayEnterAnimation()
{
    m_state = ViewState::Enter;
    m_animEnter.Restart(0.0f, 1.0f, kEnterExitDurationSec);
    m_scrollPos           = 0.0f;
    m_scrollTarget        = 0.0f;
    m_wrapScrollAdjustThisFrame = 0.0f;
    m_heldNavigationKey   = 0;
    m_heldNavigationDirection = 0;
    m_heldNavigationStart = {};
    m_repeatedRotateStepsRemaining = 0;
    m_openingTabPending = false;
    m_openingTabStart = {};
    m_selectedHwnd        = nullptr;
    m_rRepeatedRotateRate = 0.0f;
    m_showOutgoingDuringRotation = false;
    m_exitScrollSnapshot    = 0.0f;
    m_lastPaintOrder.clear();
    m_originalFrontHwnd   = m_cards.empty() ? nullptr : m_cards[0].m_hwnd;
    InvalidateDisplaySlots();
}


// ============================================================================
// Flip3DComp::RotateListPhysically
// List splice when committing fractional scroll to discrete slots (exit only).
// ============================================================================
void Flip3DComp::RotateListPhysically(bool backward)
{
    if (m_cards.size() <= 1)
        return;

    if (backward)
        std::rotate(m_cards.rbegin(), m_cards.rbegin() + 1, m_cards.rend());
    else
        std::rotate(m_cards.begin(), m_cards.begin() + 1, m_cards.end());
}

// ============================================================================
// Flip3DComp::WrapCarouselScroll
// Keep scrollPos in (-0.5, 0.5) by rotating the list at integer boundaries.
// Forward wrap decouples the outgoing card's display slot so it can finish the
// front-edge opacity ramp (slot in [-1, 0]) without jumping to the back.
// ============================================================================
void Flip3DComp::WrapCarouselScroll()
{
    if (m_cards.size() <= 1)
        return;

    m_wrapScrollAdjustThisFrame = 0.0f;

    const int N = (int)m_cards.size();
    int guard = 0;
    while (m_scrollPos >= 0.5f && guard++ <= N)
    {
        const HWND  outgoingHwnd = m_cards[0].m_hwnd;
        const float outgoingSlot = GetCardDisplaySlot(0);

        RotateListPhysically(false);
        m_scrollPos    -= 1.0f;
        m_scrollTarget -= 1.0f;
        m_wrapScrollAdjustThisFrame -= 1.0f;

        OnCarouselWrapForward(outgoingHwnd, outgoingSlot);
    }
    guard = 0;
    while (m_scrollPos <= -0.5f && guard++ <= N)
    {
        const HWND incomingHwnd = m_cards[(size_t)(N - 1)].m_hwnd;

        RotateListPhysically(true);
        m_scrollPos    += 1.0f;
        m_scrollTarget += 1.0f;
        m_wrapScrollAdjustThisFrame += 1.0f;

        OnCarouselWrapBackward(incomingHwnd);
    }
}

// ============================================================================
// Display slot helpers — continuous carousel coordinates per card
// ============================================================================
float Flip3DComp::CardListSlot(int listIndex) const
{
    return (float)listIndex - m_scrollPos;
}

float Flip3DComp::CaptureCarouselVisualSlot(int listIndex) const
{
    if (listIndex < 0 || listIndex >= (int)m_cards.size())
        return 0.0f;

    const CardModel& c = m_cards[(size_t)listIndex];

    if (c.m_wrapPhase == CarouselWrapPhase::EnteringBack)
        return CardListSlot(listIndex);

    if (c.m_wrapPhase != CarouselWrapPhase::None)
        return c.m_displaySlot;

    return CardListSlot(listIndex);
}

float Flip3DComp::GetCardDisplaySlot(int listIndex) const
{
    if (listIndex < 0 || listIndex >= (int)m_cards.size())
        return 0.0f;

    const CardModel& c = m_cards[(size_t)listIndex];

    if (m_state == ViewState::Exit || m_state == ViewState::ExitRepeatedRotate)
        return c.m_displaySlot;

    return CaptureCarouselVisualSlot(listIndex);
}

void Flip3DComp::FinishWrapPhase(CardModel& card, int listIndex)
{
    card.m_wrapPhase              = CarouselWrapPhase::None;
    card.m_wrapFadeStartListSlot  = 0.0f;
    card.m_displaySlot            = CardListSlot(listIndex);
    card.m_displaySlotValid       = true;
}

void Flip3DComp::BeginEnteringBackWrap(CardModel& card, int listIndex)
{
    card.m_wrapPhase             = CarouselWrapPhase::EnteringBack;
    card.m_wrapFadeStartListSlot = CardListSlot(listIndex);
    card.m_displaySlot           = CarouselEdgeSpan();
    card.m_displaySlotValid      = true;
}

void Flip3DComp::StepEnteringBackWrap(CardModel& card, int listIndex)
{
    const float listSlot  = CardListSlot(listIndex);
    const float span      = CarouselEdgeSpan();
    const float fadeRange = span - card.m_wrapFadeStartListSlot;

    constexpr float kWrapSettleEpsilon = 0.06f;

    if (fadeRange <= 1e-4f)
    {
        FinishWrapPhase(card, listIndex);
        return;
    }

    const float scrolled = card.m_wrapFadeStartListSlot - listSlot;
    const float t        = std::clamp(scrolled / fadeRange, 0.0f, 1.0f);
    card.m_displaySlot   = listSlot + fadeRange * (1.0f - t);

    if (t >= 1.0f - kWrapSettleEpsilon)
        FinishWrapPhase(card, listIndex);
}

void Flip3DComp::SettleWrapDisplaySlots()
{
    if (std::abs(m_scrollTarget - m_scrollPos) > kScrollSettleEpsilon)
        return;

    constexpr float kWrapSettleEpsilon = 0.06f;

    for (int k = 0; k < (int)m_cards.size(); ++k)
    {
        CardModel& c = m_cards[(size_t)k];
        const float listSlot = CardListSlot(k);

        switch (c.m_wrapPhase)
        {
        case CarouselWrapPhase::EnteringFront:
            if (std::abs(c.m_displaySlot - listSlot) < kWrapSettleEpsilon
                || c.m_displaySlot > listSlot + kWrapSettleEpsilon)
            {
                FinishWrapPhase(c, k);
            }
            break;

        case CarouselWrapPhase::ExitingFront:
            if (c.m_displaySlot <= -1.0f)
                BeginEnteringBackWrap(c, k);
            break;

        case CarouselWrapPhase::EnteringBack:
            StepEnteringBackWrap(c, k);
            break;

        default:
            break;
        }
    }
}

float Flip3DComp::CarouselEdgeSpan() const
{
    const int count = (int)m_cards.size();
    return (float)std::min(std::max(count, 2), kMaxVisibleCards);
}

void Flip3DComp::InvalidateDisplaySlots()
{
    for (auto& c : m_cards)
    {
        c.m_displaySlotValid = false;
        c.m_wrapPhase        = CarouselWrapPhase::None;
    }
}

void Flip3DComp::SyncDisplaySlotsToList()
{
    for (int k = 0; k < (int)m_cards.size(); ++k)
    {
        CardModel& c = m_cards[(size_t)k];
        if (c.m_wrapPhase != CarouselWrapPhase::None)
            continue;

        c.m_displaySlot      = CardListSlot(k);
        c.m_displaySlotValid = true;
    }
}

void Flip3DComp::AdvanceWrapDisplaySlots(float deltaScrollPos)
{
    if (deltaScrollPos == 0.0f && m_wrapScrollAdjustThisFrame == 0.0f)
        return;

    constexpr float kWrapSettleEpsilon = 0.06f;
    const float enteringFrontDelta = deltaScrollPos - m_wrapScrollAdjustThisFrame;

    for (int k = 0; k < (int)m_cards.size(); ++k)
    {
        CardModel& c = m_cards[(size_t)k];
        const float listSlot = CardListSlot(k);

        switch (c.m_wrapPhase)
        {
        case CarouselWrapPhase::ExitingFront:
            c.m_displaySlot -= deltaScrollPos;
            if (c.m_displaySlot <= -1.0f)
                BeginEnteringBackWrap(c, k);
            break;

        case CarouselWrapPhase::EnteringBack:
            StepEnteringBackWrap(c, k);
            break;

        case CarouselWrapPhase::EnteringFront:
            c.m_displaySlot -= enteringFrontDelta;
            if (std::abs(c.m_displaySlot - listSlot) < kWrapSettleEpsilon
                || c.m_displaySlot > listSlot + kWrapSettleEpsilon)
            {
                FinishWrapPhase(c, k);
            }
            break;

        default:
            break;
        }
    }
}

void Flip3DComp::OnCarouselWrapForward(HWND outgoingHwnd, float outgoingSlot)
{
    for (int k = 0; k < (int)m_cards.size(); ++k)
    {
        CardModel& c = m_cards[(size_t)k];
        if (c.m_hwnd != outgoingHwnd
            && c.m_wrapPhase == CarouselWrapPhase::EnteringFront)
        {
            FinishWrapPhase(c, k);
        }
    }

    for (auto& c : m_cards)
    {
        if (c.m_hwnd != outgoingHwnd)
            continue;

        c.m_displaySlot      = outgoingSlot;
        c.m_wrapPhase        = CarouselWrapPhase::ExitingFront;
        c.m_displaySlotValid = true;
        break;
    }

    SyncDisplaySlotsToList();
}

void Flip3DComp::OnCarouselWrapBackward(HWND incomingHwnd)
{
    for (int k = 0; k < (int)m_cards.size(); ++k)
    {
        CardModel& c = m_cards[(size_t)k];
        if (c.m_hwnd == incomingHwnd)
            continue;

        if (c.m_wrapPhase == CarouselWrapPhase::EnteringFront
            || c.m_wrapPhase == CarouselWrapPhase::ExitingFront
            || c.m_wrapPhase == CarouselWrapPhase::EnteringBack)
        {
            FinishWrapPhase(c, k);
        }
    }

    for (int k = 0; k < (int)m_cards.size(); ++k)
    {
        CardModel& c = m_cards[(size_t)k];
        if (c.m_hwnd != incomingHwnd)
            continue;

        c.m_wrapPhase        = CarouselWrapPhase::EnteringFront;
        c.m_displaySlot      = CardListSlot(k) - 0.55f;
        c.m_displaySlotValid = true;
        break;
    }

    SyncDisplaySlotsToList();
}

void Flip3DComp::FreezeCarouselVisuals()
{
    std::vector<std::pair<HWND, float>> visualByHwnd;
    visualByHwnd.reserve(m_cards.size());
    for (int k = 0; k < (int)m_cards.size(); ++k)
        visualByHwnd.emplace_back(m_cards[(size_t)k].m_hwnd,
                                    CaptureCarouselVisualSlot(k));

    WrapCarouselScroll();

    for (int k = 0; k < (int)m_cards.size(); ++k)
    {
        CardModel& c = m_cards[(size_t)k];
        c.m_wrapPhase             = CarouselWrapPhase::None;
        c.m_wrapFadeStartListSlot = 0.0f;

        for (const auto& entry : visualByHwnd)
        {
            if (entry.first == c.m_hwnd)
            {
                c.m_displaySlot = entry.second;
                break;
            }
        }

        c.m_displaySlotValid = true;
    }

    m_exitScrollSnapshot = m_scrollPos;
    m_scrollTarget       = m_scrollPos;
}

void Flip3DComp::UpdateVisualSlots(float enterProgress)
{
    if (m_state != ViewState::Exit && m_state != ViewState::ExitRepeatedRotate)
        return;

    const int cardCount = (int)m_cards.size();

    if (m_state == ViewState::ExitRepeatedRotate && m_rotateTimeline.IsActive())
    {
        for (int k = 0; k < cardCount; ++k)
            m_cards[(size_t)k].m_displaySlot = ComputeRotationDisplaySlot(k);

        return;
    }

    const bool rotationDone = m_state == ViewState::ExitRepeatedRotate
                           && !m_cards.empty()
                           && m_selectedHwnd
                           && m_cards[0].m_hwnd == m_selectedHwnd;

    if (m_state == ViewState::Exit || rotationDone)
    {
        m_scrollPos    = m_exitScrollSnapshot * enterProgress;
        m_scrollTarget = m_scrollPos;

        for (int k = 0; k < cardCount; ++k)
            m_cards[(size_t)k].m_displaySlot = CardListSlot(k);
    }

    // ExitRepeatedRotate with more steps pending: keep m_displaySlot unchanged
}

// ============================================================================
// Flip3DComp::RotateSelectedToListFront
// uDWM: after scroll settles, splice the ring so the selection is list index 0
// (e.g. A B C D E F G → C D E F G A B). Cards after C in the ring follow C at
// slots 1…, not the windows that were before C (A B).
// ============================================================================
void Flip3DComp::RotateSelectedToListFront()
{
    if (!m_selectedHwnd || m_cards.size() <= 1)
        return;

    const int idx = FindCardIndex(m_selectedHwnd);
    if (idx <= 0)
        return;

    for (int i = 0; i < idx; ++i)
        RotateListPhysically(false);
}

// ============================================================================
// Flip3DComp::CommitCarouselScroll
// Snap fractional scroll to integer slots and align the window list.
// ============================================================================
void Flip3DComp::CommitCarouselScroll()
{
    if (m_cards.size() <= 1)
    {
        m_scrollPos    = 0.0f;
        m_scrollTarget = 0.0f;
        InvalidateDisplaySlots();
        return;
    }

    AlignCarouselScrollSettled();
}

// ============================================================================
// Flip3DComp::AlignCarouselScrollSettled
// Wrap + rotate selection to list front + zero scroll (shared by commit & exit).
// ============================================================================
void Flip3DComp::AlignCarouselScrollSettled()
{
    if (m_cards.size() <= 1)
    {
        m_scrollPos    = 0.0f;
        m_scrollTarget = 0.0f;
        return;
    }

    WrapCarouselScroll();
    RotateSelectedToListFront();
    m_scrollPos    = 0.0f;
    m_scrollTarget = 0.0f;

    for (int k = 0; k < (int)m_cards.size(); ++k)
    {
        CardModel& c = m_cards[(size_t)k];
        c.m_wrapPhase             = CarouselWrapPhase::None;
        c.m_wrapFadeStartListSlot = 0.0f;
    }

    SyncDisplaySlotsToList();

    m_exitScrollSnapshot = 0.0f;
}

// ============================================================================
// Flip3DComp::RotateBy
// ============================================================================
void Flip3DComp::RotateBy(int deltaSteps)
{
    if (!deltaSteps || m_cards.size() <= 1
        || m_state == ViewState::Exit
        || m_state == ViewState::ExitRepeatedRotate)
        return;

    m_scrollTarget += (float)deltaSteps;
}

// ============================================================================
// Flip3DComp::ComputeScrollTargetForCard
// Scroll value that places listIndex at the visual front (slot ≈ 0).
// Short-path wrap is only used when it moves scrollPos in the direction that
// reduces |visualSlot| (slot = index - scrollPos increases as scrollPos falls).
// ============================================================================
float Flip3DComp::ComputeScrollTargetForCard(int listIndex) const
{
    const int N = (int)m_cards.size();
    if (N <= 1)
        return m_scrollPos;

    const float visualSlot = (float)listIndex - m_scrollPos;
    if (std::abs(visualSlot) < kScrollSettleEpsilon)
        return m_scrollPos;

    float shortDelta = visualSlot;
    if (shortDelta > (float)N * 0.5f)
        shortDelta -= (float)N;
    else if (shortDelta < -(float)N * 0.5f)
        shortDelta += (float)N;

    if (visualSlot > 0.0f)
    {
        if (shortDelta > 0.0f && std::abs(shortDelta) < std::abs(visualSlot))
            return m_scrollPos + shortDelta;
    }
    else
    {
        if (shortDelta < 0.0f && std::abs(shortDelta) < std::abs(visualSlot))
            return m_scrollPos + shortDelta;
    }

    return m_scrollPos + visualSlot;
}

// ============================================================================
// Flip3DComp::IsSelectionAtCarouselFront
// Exit scroll completes when the selection is list index 0 and scroll settled.
// ============================================================================
bool Flip3DComp::IsSelectionAtCarouselFront() const
{
    if (!m_selectedHwnd || m_cards.empty())
        return true;

    if (m_cards[0].m_hwnd != m_selectedHwnd)
        return false;

    return std::abs(m_scrollPos) < 0.05f
        && std::abs(m_scrollTarget - m_scrollPos) < 0.05f;
}

// ============================================================================
// Flip3DComp::StepCarouselScroll
// Shared smooth scroll + wrap + front-edge fade (browse and exit).
// ============================================================================
void Flip3DComp::StepCarouselScroll(float dtSeconds, bool notifyFrontChange)
{
    const float prevFront  = std::floor(m_scrollPos + 0.5f);
    const float prevScroll = m_scrollPos;
    const float a = 1.0f - std::exp(-dtSeconds / std::max(kScrollSmoothTimeSec, 1e-4f));
    m_scrollPos += (m_scrollTarget - m_scrollPos) * a;
    const float smoothDelta = m_scrollPos - prevScroll;
    WrapCarouselScroll();
    AdvanceWrapDisplaySlots(smoothDelta);

    SettleWrapDisplaySlots();

    if (notifyFrontChange)
    {
        const float newFront = std::floor(m_scrollPos + 0.5f);
        if (newFront != prevFront)
            NotifyAccessibilityFocusFront();
    }
}

// ============================================================================
// Flip3DComp::RotateToWindow
// ============================================================================
void Flip3DComp::RotateToWindow(HWND targetHwnd)
{
    if (m_cards.size() <= 1 || m_state == ViewState::Exit
        || m_state == ViewState::ExitRepeatedRotate)
        return;

    int idx = -1;
    for (int i = 0; i < (int)m_cards.size(); ++i)
    {
        if (m_cards[(size_t)i].m_hwnd == targetHwnd) { idx = i; break; }
    }
    if (idx < 0)
        return;

    m_scrollTarget = ComputeScrollTargetForCard(idx);
}

// ============================================================================
// Flip3DComp::TickSmoothScroll
// ============================================================================
void Flip3DComp::TickSmoothScroll(float dtSeconds)
{
    if (m_cards.size() <= 1 || m_state != ViewState::Interactive)
        return;

    const auto now = std::chrono::steady_clock::now();
    const float heldTime = std::chrono::duration<float>(now - m_heldNavigationStart).count();
    if (m_heldNavigationDirection != 0 && heldTime >= kHeldKeyStartDelaySec)
    {
        m_scrollTarget += (float)m_heldNavigationDirection
            * kHeldKeyRotateSpeed * dtSeconds;
    }

    if (m_wheelPendingSlots != 0)
    {
        const auto now = std::chrono::steady_clock::now();
        const float sinceWheel = std::chrono::duration<float>(now - m_lastWheelTime).count();
        if (sinceWheel > kWheelActiveTimeoutSec)
        {
            m_wheelPendingSlots = 0;
        }
        else
        {
            const float sinceKey = std::chrono::duration<float>(now - m_lastKeyProcessed).count();
            if (sinceKey >= kKeyRepeatIntervalSec)
            {
                const int step = (m_wheelPendingSlots > 0) ? 1 : -1;
                RotateBy(step);
                m_wheelPendingSlots -= step;
                m_lastKeyProcessed = now;
            }
        }
    }

    StepCarouselScroll(dtSeconds, /*notifyFrontChange=*/true);
}

// ============================================================================
// Flip3DComp::Update
// ============================================================================
void Flip3DComp::Update(float dtSeconds)
{
    if (m_thumbnailsDirty)
        OnThumbnailSourceSizeChanged();

    if (GetAsyncKeyState(VK_SHIFT) & 0x8000)
        dtSeconds *= 0.05f;

    m_animEnter.Update(dtSeconds);

    if (m_rotateTimeline.IsActive())
    {
        m_rotateTimeline.Update(dtSeconds);
        OnRotationTimeUpdated();
    }

    if (!m_rotateTimeline.IsActive())
        TickRepeatedRotate();

    const float enterProgress = EnterProgress();
    UpdateVisualSlots(enterProgress);

    TickSmoothScroll(dtSeconds);

    if (m_state == ViewState::Enter && !m_animEnter.IsActive())
    {
        m_state = ViewState::Interactive;
        InvalidateDisplaySlots();
        NotifyAccessibilityEvent(EVENT_SYSTEM_DIALOGSTART);
        NotifyAccessibilityFocusFront();
    }

    if (m_openingTabPending && m_state == ViewState::Interactive)
    {
        const auto now = std::chrono::steady_clock::now();
        const bool tabStillDown = (GetAsyncKeyState(VK_TAB) & 0x8000) != 0;
        const float pendingTime = std::chrono::duration<float>(now - m_openingTabStart).count();
        if (!tabStillDown)
        {
            m_openingTabPending = false;
        }
        else if (pendingTime >= kOpeningTabDelaySec)
        {
            m_openingTabPending = false;
            OnKey(true, VK_TAB, 0);
            m_heldNavigationStart = now
                - std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                    std::chrono::duration<float>(kHeldKeyStartDelaySec));
        }
    }

    UpdateCamera(enterProgress);
    UpdateCards(enterProgress);

    if (m_dcompDevice)
        m_dcompDevice->Commit();

    if (m_state == ViewState::Exit && !m_animEnter.IsActive())
        DestroyWindow(m_hwnd);

    if (m_state == ViewState::ExitRepeatedRotate
        && !m_animEnter.IsActive()
        && !m_rotateTimeline.IsActive()
        && !m_cards.empty()
        && m_cards[0].m_hwnd == m_selectedHwnd)
    {
        DestroyWindow(m_hwnd);
    }
}

// ============================================================================
// Flip3DComp::BuildCameraMatrix
// View × Proj × Viewport — shared by card transforms and hit-testing.
// ============================================================================
Matrix4x4 Flip3DComp::BuildCameraMatrix(float enterProgress) const
{
    auto view     = BuildViewMatrix(enterProgress);
    auto proj     = BuildProjMatrix(1.0f, enterProgress);
    auto viewport = Math::BuildViewportMatrix(m_monW, m_monH, m_viewX, m_viewY);
    return Math::Multiply(Math::Multiply(view, proj), viewport);
}

// ============================================================================
// Flip3DComp::UpdateCamera
// Scene root stays identity. Each card bakes Model × View × Proj × VP on its
// container visual (same as the pre-SPATIAL single-matrix path).
// ============================================================================
void Flip3DComp::UpdateCamera(float /*enterProgress*/)
{
    if (!m_sceneVisual)
        return;

    m_sceneVisual->SetTransform(Math::Identity());
}

float Flip3DComp::ComputeFlatDepthRank(float slot, float enterProgress,
                                          int listIndex) const
{
    const float p = enterProgress;

    if (m_state == ViewState::Exit || m_state == ViewState::ExitRepeatedRotate)
        return slot;

    if (p < 1.0f - 1e-5f)
    {
        if (listIndex >= 0 && listIndex < (int)m_cards.size())
        {
            const CardModel& c = m_cards[(size_t)listIndex];
            if (m_selectedHwnd && c.m_hwnd == m_selectedHwnd)
                return -1.0f;
            return (float)c.m_initialCarouselIndex;
        }
    }

    return slot;
}

// ============================================================================
// Flip3DComp::ComputePaintKey
// DirectComposition has no depth buffer — sibling order must track carousel depth.
// Blending slot with desktop rank mid-exit causes keys to cross and flicker.
// ============================================================================
float Flip3DComp::ComputePaintKey(float slot, float enterProgress,
                                     int listIndex) const
{
    const float p = enterProgress;

    auto flatRank = [this, listIndex](int k) -> float
    {
        if (k >= 0 && k < (int)m_cards.size())
        {
            const CardModel& c = m_cards[(size_t)k];
            if (m_selectedHwnd && c.m_hwnd == m_selectedHwnd)
                return -1.0f;
            return (float)c.m_initialCarouselIndex;
        }
        return (float)listIndex;
    };

    if (m_state == ViewState::Exit || m_state == ViewState::ExitRepeatedRotate)
        return slot;

    if (m_state == ViewState::Enter)
        return p * slot + (1.0f - p) * flatRank(listIndex);

    return slot;
}

// ============================================================================
// Flip3DComp::UpdateCards
// ============================================================================
void Flip3DComp::UpdateCards(float enterProgress)
{
    if (!m_sceneVisual)
        return;

    const int N = (int)m_cards.size();
    if (N == 0)
        return;

    const float p      = enterProgress;
    const auto  camera = BuildCameraMatrix(p);
    const int   maxVis = kMaxVisibleCards;

    const bool scrollPath = (m_state == ViewState::Interactive);
    const bool exitRotate = (m_state == ViewState::ExitRepeatedRotate);
    if (scrollPath)
        SyncDisplaySlotsToList();

    struct CardDraw { CardModel* card; float slot; float opacity; float key; int listIndex; };
    std::vector<CardDraw> draws;
    draws.reserve((size_t)N);

    for (int k = 0; k < N; ++k)
    {
        CardModel& c = m_cards[(size_t)k];
        if (!c.m_containerVisual)
            continue;

        const float slot    = GetCardDisplaySlot(k);
        const float opacity = ComputeUpdateAlpha(c, p, slot);

        const float frontCull = exitRotate ? -2.0f
                              : (m_state == ViewState::Exit) ? -1.5f
                              :                                -1.0f;

        if (scrollPath)
        {
            const bool wrapDecoupled = (c.m_wrapPhase == CarouselWrapPhase::ExitingFront
                                     || c.m_wrapPhase == CarouselWrapPhase::EnteringFront);
            const float span = CarouselEdgeSpan();
            if (!wrapDecoupled && slot >= (float)maxVis)
            {
                c.m_containerVisual->SetVisible(FALSE);
                continue;
            }
            if (wrapDecoupled && (slot < -1.5f || slot > span + 0.5f))
            {
                c.m_containerVisual->SetVisible(FALSE);
                continue;
            }
            if (opacity < 0.01f)
            {
                c.m_containerVisual->SetVisible(FALSE);
                continue;
            }
        }
        else if (exitRotate)
        {
            if (!ShouldDrawRotationCard(k) && opacity < 0.01f)
            {
                c.m_containerVisual->SetVisible(FALSE);
                continue;
            }
            if (opacity < 0.01f)
            {
                c.m_containerVisual->SetVisible(FALSE);
                continue;
            }
        }
        else if (slot >= (float)maxVis
              || (slot <= frontCull && opacity < 0.01f))
        {
            c.m_containerVisual->SetVisible(FALSE);
            continue;
        }

        const float key = ComputePaintKey(slot, p, k);
        draws.push_back({ &c, slot, opacity, key, k });
    }

    for (int k = 0; k < N; ++k)
    {
        CardModel& c = m_cards[(size_t)k];
        if (!c.m_containerVisual)
            continue;
        bool listed = false;
        for (const auto& d : draws)
            if (d.card == &c) { listed = true; break; }
        if (!listed)
            c.m_containerVisual->SetVisible(FALSE);
    }

    std::sort(draws.begin(), draws.end(),
        [](const CardDraw& a, const CardDraw& b)
        {
            if (a.key != b.key)
                return a.key > b.key;
            return a.listIndex > b.listIndex;
        });

    std::vector<HWND> paintOrder;
    paintOrder.reserve(draws.size());
    for (const auto& d : draws)
        paintOrder.push_back(d.card->m_hwnd);

    const bool orderChanged = (paintOrder != m_lastPaintOrder);
    if (orderChanged)
    {
        for (auto& d : draws)
            m_sceneVisual->RemoveVisual(d.card->m_containerVisual.Get());
        m_lastPaintOrder = std::move(paintOrder);
    }

    IDCompositionVisual* prevVis = nullptr;
    for (auto& d : draws)
    {
        d.card->m_containerVisual->SetVisible(TRUE);
        d.card->m_containerVisual->SetOpacity(d.opacity);

        const float t = ComputeCarouselBezierT(d.slot);
        const float flatRank = ComputeFlatDepthRank(d.slot, p, d.listIndex);
        auto model = BuildModelMatrix(*d.card, t, p, flatRank);
        d.card->m_containerVisual->SetTransform(Math::Multiply(model, camera));

        if (orderChanged)
        {
            if (prevVis)
                m_sceneVisual->AddVisual(d.card->m_containerVisual.Get(), TRUE, prevVis);
            else
                m_sceneVisual->AddVisual(d.card->m_containerVisual.Get(), FALSE, nullptr);
            prevVis = d.card->m_containerVisual.Get();
        }
    }
}

// ============================================================================
// Flip3DComp::ComputeCarouselEdgeOpacity
// Continuous carousel edge roll-off used by the smooth-scroll path. Matches the
// uDWM steady-state alpha: full inside, 0.5 at the back boundary, fading toward
// the camera-side (slot < 0) as a card flies out the front.
// ============================================================================
float Flip3DComp::ComputeCarouselEdgeOpacity(float slot) const
{
    if (slot < 0.0f)
        return std::clamp(1.0f + slot, 0.0f, 1.0f);

    const float span = CarouselEdgeSpan();
    if (slot >= span)
        return 0.0f;
    if (slot >= span - 2.0f)
        return (span - slot) / 2.0f;
    return 1.0f;
}

// ============================================================================
// Flip3DComp::ComputeUpdateAlpha
// ============================================================================
float Flip3DComp::ComputeUpdateAlpha(const CardModel& card, float enterProgress,
                                        float carouselSlot) const
{
    float opacitySlot = carouselSlot;
    if (m_state == ViewState::Interactive
        && card.m_wrapPhase == CarouselWrapPhase::EnteringBack)
    {
        opacitySlot = card.m_displaySlot;
    }

    float rotationOpacity = ComputeCarouselEdgeOpacity(opacitySlot);
    float enterScale      = 1.0f;

    const bool exitRotateFlattenTail =
        m_state == ViewState::ExitRepeatedRotate
        && !m_rotateTimeline.IsActive()
        && !m_cards.empty()
        && m_cards[0].m_hwnd == m_selectedHwnd;

    if ((m_state == ViewState::Exit || exitRotateFlattenTail)
        && (int)m_cards.size() > kMaxVisibleCards
        && carouselSlot >= (float)kMaxVisibleCards - 1.0f)
    {
        rotationOpacity = kDesktopWashOpacityScale;
    }

    if (m_state == ViewState::ExitRepeatedRotate && m_rotateTimeline.IsActive())
    {
        const int idx = FindCardIndex(card.m_hwnd);
        if (idx >= 0)
            return ComputeRotationAlpha(card, enterProgress, idx, carouselSlot);
    }

    if (std::abs(enterProgress - 1.0f) >= 1e-6f)
    {
        if (std::abs(rotationOpacity - 1.0f) >= 1e-6f)
            rotationOpacity = enterProgress * rotationOpacity
                            + (1.0f - enterProgress);

        if (card.m_isMinimized)
            enterScale = enterProgress * enterProgress;
        else if (card.m_isShellDesktop)
            enterScale = enterProgress;
    }

    return std::clamp(rotationOpacity * enterScale, 0.0f, 1.0f);
}

// ============================================================================
// Flip3DComp::ComputeCarouselPathDenom
// uDWM UpdateModels: m_cWindows = max(count, 5), capped by max visible.
// ============================================================================
float Flip3DComp::ComputeCarouselPathDenom() const
{
    const int N = (int)m_cards.size();
    return (float)std::min(std::max(N, 5), kMaxVisibleCards);
}

// ============================================================================
// Flip3DComp::ComputeCarouselBezierT
// flip3d/uDWM: zIndex = -pathRelative; t = 1 - (zIndex - 0.5) / -m_cWindows
// (pathRelative == carousel slot: 0 = front, larger = further back)
// ============================================================================
float Flip3DComp::ComputeCarouselBezierT(float slot) const
{
    const float denom  = ComputeCarouselPathDenom();
    const float zIndex = -slot;
    // uDWM GetWorldFromParametric evaluates the quadratic path with NO clamp.
    // Windows past the view are culled by the caller, so in practice only the
    // back-edge card (t slightly < 0, against P0) and the front/outgoing card
    // (slot < 0 → t > 1, extending past P2 toward the camera) leave [0,1].
    return 1.0f - ((zIndex - 0.5f) / -denom);
}

// ============================================================================
// Flip3DComp::BuildViewMatrix
// ============================================================================
Matrix4x4 Flip3DComp::BuildViewMatrix(float p) const
{
    using namespace Math;

    float yaw   = kCameraFinalRotateY * p;
    float pitch = kCameraFinalRotateX * p;
    float roll  = kCameraFinalRotateZ * p;

    auto T1  = Translation(0, 0, 1);
    auto Rot = RotationRollPitchYaw(pitch, yaw, roll);
    auto T2  = Translation(0, 0, -1);
    auto T3  = Translation(-kCameraFinalTranslateX * p,
                            -kCameraFinalTranslateY * p,
                            -kCameraFinalTranslateZ * p);
    auto LA  = LookAtRH({0, 0, 1}, {0, 0, 0}, {0, 1, 0});

    auto V = T1;
    V = Multiply(V, Rot);
    V = Multiply(V, T2);
    V = Multiply(V, T3);
    V = Multiply(V, LA);
    return V;
}

// ============================================================================
// Flip3DComp::BuildProjMatrix
// ============================================================================
Matrix4x4 Flip3DComp::BuildProjMatrix(float /*aspect*/, float p) const
{
    float nearExtent = p * kNearPlaneEdgeSize + (1.0f - p);
    return Math::PerspectiveRH(nearExtent, 1.0f, kNearPlaneDistance, 50.0f);
}

// ============================================================================
// Flip3DComp::BuildModelMatrix
// uDWM UpdateModels / flip3d GetWorldFromParametric: carousel pathRelative for
// both 3D bezier and flatZ; lerp to desktop when enterProgress != 1.
// ============================================================================
Matrix4x4 Flip3DComp::BuildModelMatrix(const CardModel& c, float t,
                                          float enterProgress,
                                          float flatDepthRank) const
{
    using namespace Math;

    Vec3  cv     = Bezier(t);
    float fullW  = std::abs(c.m_targetSize.x);
    float fullH  = std::abs(c.m_targetSize.y);

    Vec3 pos3d = { cv.x, cv.y + fullH, cv.z };

    float p = std::clamp(enterProgress, 0.0f, 1.0f);
    const float flatZ = (-flatDepthRank) / 10000.0f;

    float tx, ty, tz, worldW, worldH;

    if (p >= 1.0f - 1e-5f)
    {
        tx = pos3d.x;  ty = pos3d.y;  tz = pos3d.z;
        worldW = fullW;
        worldH = fullH;
    }
    else if (p <= 1e-5f)
    {
        tx = c.m_flatPos.x;
        ty = c.m_flatPos.y;
        tz = flatZ;
        worldW = c.m_flatSize.x;
        worldH = c.m_flatSize.y;
    }
    else
    {
        Vec3 pos2d = { c.m_flatPos.x, c.m_flatPos.y, flatZ };
        tx     = Lerp(pos2d.x, pos3d.x, p);
        ty     = Lerp(pos2d.y, pos3d.y, p);
        tz     = Lerp(pos2d.z, pos3d.z, p);
        worldW = Lerp(c.m_flatSize.x, fullW, p);
        worldH = Lerp(c.m_flatSize.y, fullH, p);
    }

    float sx =  worldW / (float)std::max(c.m_srcWidth,  1);
    float sy = -worldH / (float)std::max(c.m_srcHeight, 1);

    auto S = Scale(sx, sy, 1.0f);
    auto T = Translation(tx, ty, tz);
    auto M = Multiply(S, T);

    if (m_rtl)
    {
        auto Mirror = Multiply(
            Scale(-1.0f, 1.0f, 1.0f),
            Translation((float)c.m_srcWidth, 0.0f, 0.0f));
        M = Multiply(Mirror, M);
    }

    return M;
}
