@echo off
set FIREBASE_CREDENTIALS=C:\Users\tonil\Desktop\tcc\lead-manager-54595-firebase-adminsdk-fbsvc-375221693a.json
set JWT_SECRET=change-this-to-a-strong-secret-in-production
set PORT=8080
echo Starting LeadManager on http://localhost:%PORT%
go run ./cmd/server/main.go
