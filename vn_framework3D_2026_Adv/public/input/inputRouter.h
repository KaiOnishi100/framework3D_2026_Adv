//#pragma once
//
////未使用
////--------------------------------------------
//// 入力の振り分け係
//// - Game用とUI用を別のInputHandlerで持つ
//// - 今どっちを動かすか（InputContext）を切り替える
////--------------------------------------------
//class InputRouter
//{
//public:
//    void SetContext(InputContext ctx) { mContext = ctx; }
//    InputContext GetContext() const { return mContext; }
//
//    InputHandler& Game() { return mGame; }
//    InputHandler& UI() { return mUI; }
//
//    // Game側：IControllable に投げる
//    void HandleGame(IControllable* target);
//    // UI側：UIの受け手に投げる
//    template<class UIReceiver>
//    void HandleUI(UIReceiver* uiTarget);
//
//private:
//    InputContext mContext = InputContext::Game;
//    InputHandler mGame;
//    InputHandler mUI;
//};