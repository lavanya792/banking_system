MiniBank – Lightweight Banking Dashboard

MiniBank is a simple fintech-style banking dashboard where users can securely log in, view accounts, deposit/withdraw money, transfer funds, and track financial activity through clean visual charts.

⸻

✨ Features
	•	🔐 User Authentication – Login & signup with backend API.
	•	💳 Account Overview – View all accounts with real-time balances.
	•	💸 Transactions – Deposit, withdraw, transfer money.
	•	📊 Analytics Dashboard –
	•	Daily net flow chart
	•	Monthly summary
	•	Transaction trend line chart
	•	Transaction-type pie chart
	•	🧮 Monthly Financial Summary – Deposits, withdrawals, net flow.
	•	📁 Transaction History – Sortable tabular history per account.
	•	⚡ Fast & Optimized – API caching reduces lag and improves UI speed.
	•	🌤 Clean Light Theme – Soft colors, card UI, smooth layout.

⸻

📦 Tech Stack
	•	Frontend: Streamlit
	•	Backend: Python (FastAPI / Flask)
	•	Database: SQLite
	•	Charts: Matplotlib
	•	API Communication: REST (JSON)

⸻

🚀 How It Works
	1.	User logs in → backend verifies credentials.
	2.	Dashboard loads accounts & cached transactions.
	3.	Charts generate from last 30 days + monthly data.
	4.	User performs deposit/withdraw/transfer → UI refreshes instantly.
	5.	History page shows detailed transaction records.

⸻

📁 Project Structure
	•	app.py → Main Streamlit frontend
	•	backend_clean/server → API server
	•	bank.db → SQLite database
	•	accounts, users, transactions tables

⸻

📝 Notes
	•	Works fully offline with local backend API.
	•	UI is optimized for smooth performance.
	•	Charts auto-update after each financial action.
