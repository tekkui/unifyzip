# UnifyZip
[![CodeFactor](https://www.codefactor.io/repository/github/tekkui/unifyzip/badge)](https://www.codefactor.io/repository/github/tekkui/unifyzip)
[![Build status](https://ci.appveyor.com/api/projects/status/28xirs01hf1xgq15/branch/master?svg=true)](https://ci.appveyor.com/project/tekkui/unifyzip/branch/master)
[![License: MIT](https://img.shields.io/badge/License-MIT-lightgrey.svg)](https://opensource.org/licenses/MIT)
[![Github Releases All](https://img.shields.io/github/downloads/tekkui/unifyzip/total.svg)](https://github.com/tekkui/unifyzip/releases "All Releases")

UnifyZip 書庫統一 Unicode版

## 使い方
- [https://tekkui.github.io/unifyzip/](https://tekkui.github.io/unifyzip/)

## CI
- [AppVeyor](https://ci.appveyor.com/project/tekkui/unifyzip/branch/master)

## Code Review
- [CodeFactor](https://www.codefactor.io/repository/github/tekkui/unifyzip)

## 開発
### 環境
- [Visual Studio Community 2017](https://www.visualstudio.com/)
- [Boost Version 1.69.0](https://www.boost.org/)
### 準備

boostをAppVeyorに合わせて配置する。
- Include　C:\Libraries\boost_1_69_0
- Lib　C:\Libraries\boost_1_69_0\lib32-msvc-14.1

### 詳細

UnifyZip.slnをVisual Studioで開いてビルドする。

はじめて開いた時に、"プロジェクトの再ターゲット"のダイアログが表示されるが、キャンセルしてよい。

Debugでビルドすると、"エラー	LNK1281	SAFESEH イメージを生成できません。"が発生するが、exeは作成される。

minizip、zlibvcのプロジェクトで、"リンカー"の"詳細設定"にある"安全な例外ハンドラーを含むイメージ"を"いいえ(/SAFESEH:NO)"に設定する事で解消する。
