# MSLogin Protocol Analysis

## 步骤二：通过设备码登录

`https://login.live.com/oauth20_remoteconnect.srf?lc=2052`

- [x] otc
- [ ] canary
- [x] PPFT
- [ ] hpgrequestid
- [ ] i19

## 步骤三：提交用户名和密码

`https://login.live.com/ppsecure/post.srf?mkt=ZH-CN&contextid=6CD30C928825AA2A&opid=E484C56761F45820&bk=1770377710&uaid=121de56b3f0c4bb7bdef39dcea3bcec0&pid=15343`

- [ ] ps
- [ ] psRNGCDefaultType
- [ ] psRNGCEntropy
- [ ] psRNGCSLK
- [ ] canary
- [ ] ctx
- [ ] hpgrequestid
- [x] PPFT
- [x] PPSX
- [ ] NewUser
- [ ] FoundMSAs
- [ ] fspost
- [ ] i21
- [ ] CookieDisclosure
- [x] IsFidoSupported
- [x] isSignupPost
- [ ] isRecoveryAttemptPost
- [ ] i13
- [x] login
- [x] loginfmt
- [ ] type
- [x] LoginOptions
- [ ] lrt
- [ ] lrtPartition
- [ ] hisRegion
- [ ] hisScaleUnit
- [ ] cpr
- [x] passwd

## `https://login.live.com/oauth20_remoteconnect.srf?uaid=121de56b3f0c4bb7bdef39dcea3bcec0&opid=E484C56761F45820&mkt=ZH-CN&opidt=1770377729`

GET 请求，无需参数

## `https://login.live.com/ppsecure/post.srf?mkt=ZH-CN&uaid=121de56b3f0c4bb7bdef39dcea3bcec0&pid=15343&opid=E484C56761F45820&route=C521_SN1`

- [x] PPFT
- [ ] canary
- [x] LoginOptions
- [x] type
- [ ] hpgrequestid
- [ ] ctx
