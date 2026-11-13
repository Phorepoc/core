// Copyright The Phorepoc Authors
// SPDX-License-Identifier: Apache-2.0
// hb_s3feedback.h -- OURS (Symbian^4 S4.4, docs/phorepoc/symbian4-build.md): Orbit's feedback player names the S^4
// logical/continuous touch-feedback types; our tactile feedback server and touchlogicalfeedback.h are S^3's. Each
// S^4 type is played as the nearest S^3 one. Force-included into HbFeedback only (components_s4.py).
#ifndef HB_S3FEEDBACK_H
#define HB_S3FEEDBACK_H
#define ETouchFeedbackBasicItem           ETouchFeedbackBasicButton
#define ETouchFeedbackSensitiveItem       ETouchFeedbackSensitiveList
#define ETouchFeedbackBounceEffect        ETouchFeedbackBoundaryList
#define ETouchFeedbackPopupOpen           ETouchFeedbackPopUp
#define ETouchFeedbackPopupClose          ETouchFeedbackPopUp
#define ETouchFeedbackBasicSlider         ETouchFeedbackSlider
#define ETouchFeedbackSensitiveSlider     ETouchFeedbackSlider
#define ETouchFeedbackStopFlick           ETouchFeedbackFlick
#define ETouchFeedbackBasicKeypad         ETouchFeedbackCharacterInputButton
#define ETouchFeedbackSensitiveKeypad     ETouchFeedbackSensitiveInput
#define ETouchFeedbackEditor              ETouchFeedbackEdit
#define ETouchFeedbackItemDrop            ETouchFeedbackBasicButton
#define ETouchFeedbackItemMoveOver        ETouchFeedbackSensitiveList
#define ETouchFeedbackItemPick            ETouchFeedbackBasicButton
#define ETouchFeedbackItemScroll          ETouchFeedbackSensitiveList
#define ETouchFeedbackLongPress           ETouchFeedbackLongTap
#define ETouchFeedbackMultipleCheckbox    ETouchFeedbackCheckbox
#define ETouchFeedbackMultitouchActivate  ETouchFeedbackMultiTouchRecognized
#define ETouchFeedbackRotateStep          ETouchFeedbackSensitive
#define ETouchContinuousPinch             ETouchContinuousSmooth
#define ETouchContinuousPopup             ETouchContinuousSmooth
#endif
