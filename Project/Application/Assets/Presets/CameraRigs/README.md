# カメラリグ

シーンの状態（誰がどこにいるか）からカメラの構図を毎フレーム決め直す設定です。
時刻で動かすカットシーンは `../CameraClips/`（シーケンス）のほうを使ってください。

エディタの `Camera Editor → リグ` タブで編集・保存します。手で書く場合、
Vector3 は **配列** `[x, y, z]` です（オブジェクトではありません）。

コードからは名前で呼びます。

```cpp
CameraRig::Activate("GameCamera");

CameraRigActivateOptions options;
options.blendSeconds = 1.5f;      // 今の構図から 1.5 秒かけて移る
options.useUnscaledTime = true;   // ヒットストップ中も進める
CameraRig::Activate("Train_CloseUp", options);

CameraRig::Deactivate();          // ゲーム側の追従へ返す
```

## GameCamera

`GameComponents::CameraManagerComponent` の通常追従を、そのまま写したものです。
列車とビルダーの中間を捉え、2 人が離れるほど視野角を広げます。

| | 値 | 元コードの対応 |
|---|---|---|
| Body | FrameTargets{ Train, RailBuilder } | `desiredFocusPosition` |
| オフセット | (0, 20, -18) | `cameraOffset_` の既定値 |
| 寄り | (0.4, 0.5, 0.5) | `focusRatio_ = 0.4f`（X だけに効く） |
| Aim | FrameTargets{ Train, RailBuilder } | `LookAt(smoothedFocusPosition_)` |
| Lens | DistanceToFov 5m→35 度 / 30m→70 度 | `minFov/maxFov`, `min/maxTargetDistance` |
| 減衰 | 位置 3.0 / 注視先 3.0 / 視野角 3.0 / **向き 0.0** | `followSpeed_ = 3.0f` |

**寄りが軸ごとなのが要点。** 元コードは Y と Z を常に中点にしたまま、X だけを
`focusRatio` で寄せます。3 軸そろえて 0.4 にすると Y/Z がずれるので合いません。

**向きの減衰が 0 なのも要点。** 元コードは回転を鈍らせておらず、滑らかにした注視点から
毎フレーム `LookAt` しているだけです。位置・注視先・視野角の減衰だけで同じ滑らかさに
なります。ここに値を入れると 1 フレームぶん遅れて一致しなくなります。

検証済み: 対象を動かしながら 300 フレーム突き合わせて、位置 0.00000 m /
視野角 0.00002 度 / 向き 0.00000 度。実機でも 2 つの進行状況で一致を確認しました。

## Train_CloseUp

`CameraManagerComponent::BeginTrainCloseUp(duration, distanceScale)` に対応します。
既定の `distanceScale = 0.3` を前提に、オフセットは `(0, 20, -18) * 0.3 = (0, 6, -5.4)`。

呼び出しは `blendSeconds` を元コードの `duration`（既定 1.5 秒）に合わせ、
`useUnscaledTime = true` にしてください。元コードも `UnscaledDeltaTime` で進めています。

リグ側の減衰はすべて 0 です。寄りの動きは減衰ではなく繋ぎ（ブレンド）が作ります。

> 元コードの補間は smoothstep `t*t*(3-2t)`、リグの繋ぎは EaseInOutCubic です。
> どちらも両端で速度 0 になる曲線なので体感はほぼ同じですが、厳密には一致しません。
> このリグは実機で突き合わせていません（作成時点のブランチにこの機能が無いため）。
