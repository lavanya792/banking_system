# app.py — MiniBank (Medium UI, Matplotlib charts, Monthly summary, cached)
# Run: streamlit run app.py

import streamlit as st
import requests
import pandas as pd
import matplotlib.pyplot as plt
from datetime import datetime, timedelta
import calendar

# ---------------- CONFIG ----------------
BASE_URL = "http://localhost:8080"
REQUEST_TIMEOUT = 6

st.set_page_config(page_title="MiniBank Dashboard", layout="wide", page_icon="💳")

# ---------------- LIGHT CSS (small) ----------------
st.markdown("""
<style>
:root{
  --bg: #f5f7fb;
  --card: #ffffff;
  --muted: #6b7280;
  --accent: #2643d1;
}
html, body, .stApp { background: var(--bg); color: #0b1330; font-family: Inter, system-ui, -apple-system, "Segoe UI", Roboto; }
.card { background: var(--card); padding: 16px; border-radius: 12px; box-shadow: 0 6px 20px rgba(20,30,80,0.06); border:1px solid rgba(10,20,60,0.04); }
.header { display:flex; justify-content:space-between; align-items:center; gap:12px; margin-bottom:14px; }
.title { font-size:22px; font-weight:700; color:#112; }
.subtitle { color:var(--muted); font-size:13px; }
.summary-row { display:flex; gap:12px; margin-bottom:12px; }
.summary-item { flex:1; padding:12px; border-radius:10px; background:linear-gradient(180deg,#ffffff,#fbfdff); }
.small-muted { color:var(--muted); font-size:13px; }
.metric { font-size:20px; font-weight:700; color:var(--accent); margin-top:6px; }
.navbar { display:flex; gap:8px; margin-bottom:12px; }
.stButton>button { background:#ffffff; padding:8px 12px; border-radius:10px; border:1px solid #e6e9fb; font-weight:600; }
.stButton>button:hover { background:#eef2ff; }
</style>
""", unsafe_allow_html=True)

# ---------------- API HELPERS ----------------
def api_get(path):
    try:
        r = requests.get(f"{BASE_URL}{path}", timeout=REQUEST_TIMEOUT)
        try:
            return r.json()
        except:
            return {"status": "error", "detail": r.text[:400]}
    except Exception as e:
        return {"status": "error", "detail": str(e)}

def api_post(path, payload):
    try:
        r = requests.post(f"{BASE_URL}{path}", json=payload, timeout=REQUEST_TIMEOUT)
        try:
            return r.json()
        except:
            return {"status": "error", "detail": r.text[:400]}
    except Exception as e:
        return {"status": "error", "detail": str(e)}

# ---------------- CACHED FETCHERS ----------------
@st.cache_data(ttl=10)
def fetch_accounts_cached(user_id):
    if not user_id:
        return []
    res = api_get(f"/accounts/{user_id}")
    if isinstance(res, list):
        return res
    return []

@st.cache_data(ttl=10)
def fetch_transactions_cached(account_number):
    if not account_number:
        return []
    res = api_get(f"/transactions/{account_number}")
    if isinstance(res, list):
        return res
    return []

# ---------------- SESSION INIT ----------------
if "page" not in st.session_state:
    st.session_state.page = "login"
if "user_id" not in st.session_state:
    st.session_state.user_id = None

# ---------------- NAVBAR ----------------
def navbar():
    st.markdown("<div class='navbar'>", unsafe_allow_html=True)

    cols = st.columns([1,1,1,1,1])

    with cols[0]:
        if st.button("Dashboard", key="nav_dashboard"):
            st.session_state.page = "dashboard"

    with cols[1]:
        if st.button("Deposit", key="nav_deposit"):
            st.session_state.page = "deposit"

    with cols[2]:
        if st.button("Withdraw", key="nav_withdraw"):
            st.session_state.page = "withdraw"

    with cols[3]:
        if st.button("Transfer", key="nav_transfer"):
            st.session_state.page = "transfer"

    with cols[4]:
        if st.button("History", key="nav_history"):
            st.session_state.page = "history"

    st.markdown("</div>", unsafe_allow_html=True)

# ---------------- TX TYPE HELPER ----------------
def determine_tx_type(tx):
    f = tx.get("from") if tx.get("from") not in (None, "") else None
    t = tx.get("to") if tx.get("to") not in (None, "") else None

    if f is None and t is not None:
        return "Deposit"
    if t is None and f is not None:
        return "Withdraw"
    if f is not None and t is not None:
        return "Transfer"
    return "Unknown"

# ---------------- PAGES ----------------

# ----------- LOGIN -----------
def login_page():
    st.title("Login")
    email = st.text_input("Email")
    pwd = st.text_input("Password", type="password")

    if st.button("Login", key="login_btn"):
        res = api_post("/login", {"email": email, "password": pwd})
        if isinstance(res, dict) and res.get("status") == "ok":
            st.success("Login successful")
            st.session_state.user_id = int(res.get("user_id"))
            st.session_state.page = "dashboard"
            st.rerun()
        else:
            st.error("Login failed: " + str(res.get("detail", res.get("reason", ""))))

    if st.button("Go to Signup", key="goto_signup"):
        st.session_state.page = "signup"
        st.rerun()

# ----------- SIGNUP ----------
def signup_page():
    st.title("Signup")
    email = st.text_input("Email")
    pwd = st.text_input("Password", type="password")

    if st.button("Create Account", key="signup_create"):
        res = api_post("/signup", {"email": email, "password": pwd})
        if isinstance(res, dict) and res.get("status") == "ok":
            st.success("Signup complete — login now.")
            st.session_state.page = "login"
            st.rerun()
        else:
            st.error("Signup failed: " + str(res))

    if st.button("Back to Login", key="signup_back"):
        st.session_state.page = "login"
        st.rerun()

# ----------- DASHBOARD ----------
def dashboard_page():
    navbar()
    st.title("Dashboard")

    user = st.session_state.user_id
    accounts = fetch_accounts_cached(user)

    total_balance = sum(float(a.get("balance", 0)) for a in accounts)

    # Build transaction list
    all_tx = []
    for a in accounts:
        txs = fetch_transactions_cached(a.get("account_number"))
        for t in txs:
            try:
                all_tx.append({
                    "from": t.get("from"),
                    "to": t.get("to"),
                    "amount": float(t.get("amount", 0.0)),
                    "time": pd.to_datetime(t.get("time")),
                    "account": a.get("account_number")
                })
            except:
                continue

    df = pd.DataFrame(all_tx)

    # Monthly summary
    now = datetime.now()
    start_month = datetime(now.year, now.month, 1)
    end_month = datetime(now.year + (now.month == 12), (now.month % 12) + 1, 1)

    if not df.empty:
        month_df = df[(df["time"] >= start_month) & (df["time"] < end_month)].copy()
        month_df["tx_type"] = month_df.apply(determine_tx_type, axis=1)
        deposits = month_df[month_df["tx_type"] == "Deposit"]["amount"].sum()
        withdraws = month_df[month_df["tx_type"] == "Withdraw"]["amount"].sum()
        net_flow = deposits - withdraws
    else:
        deposits = withdraws = net_flow = 0

    # Summary cards
    st.markdown("<div class='summary-row'>", unsafe_allow_html=True)
    st.markdown(f"<div class='summary-item card'><div class='small-muted'>Total Balance</div><div class='metric'>₹{total_balance:,.2f}</div></div>", unsafe_allow_html=True)
    st.markdown(f"<div class='summary-item card'><div class='small-muted'>Deposits ({calendar.month_name[now.month]})</div><div class='metric'>₹{deposits:,.2f}</div></div>", unsafe_allow_html=True)
    st.markdown(f"<div class='summary-item card'><div class='small-muted'>Withdrawals ({calendar.month_name[now.month]})</div><div class='metric'>₹{withdraws:,.2f}</div></div>", unsafe_allow_html=True)
    st.markdown(f"<div class='summary-item card'><div class='small-muted'>Net Flow</div><div class='metric'>₹{net_flow:,.2f}</div></div>", unsafe_allow_html=True)
    st.markdown("</div>", unsafe_allow_html=True)

    # Accounts list
    st.subheader("Accounts")
    for a in accounts:
        st.write(f"• **{a['account_number']}** — {a['account_type']} — ₹{a['balance']}")

    # ------------------ CHARTS ------------------
    st.subheader("Charts & Insights")

    if df.empty:
        st.info("No transactions yet")
        return

    # ========== 1️⃣ PIE CHART – Deposit vs Withdraw vs Transfer ==========
    fig_pie, ax_pie = plt.subplots(figsize=(5, 5))
    fig_pie.set_facecolor('#ffffff')

    tx_counts = df.copy()
    tx_counts["tx_type"] = tx_counts.apply(determine_tx_type, axis=1)
    pie_data = tx_counts["tx_type"].value_counts()

    ax_pie.pie(
        pie_data.values,
        labels=pie_data.index,
        autopct='%1.1f%%',
        startangle=90
    )
    ax_pie.set_title("Transaction Type Distribution")
    st.pyplot(fig_pie)


    # ========== 2️⃣ LINE CHART – Transaction amounts over time ==========
    df_sorted = df.sort_values("time")

    fig_line, ax_line = plt.subplots(figsize=(8, 3.8))
    ax_line.plot(df_sorted["time"], df_sorted["amount"], marker="o")
    ax_line.set_title("Transaction Amount Trend")
    ax_line.set_xlabel("Date")
    ax_line.set_ylabel("Amount (₹)")
    ax_line.tick_params(axis='x', rotation=45)

    st.pyplot(fig_line)


    # ========== 3️⃣ BAR CHART – Daily net flow (Last 30 days) ==========
    last30 = df[df["time"] >= now - timedelta(days=30)].copy()

    if not last30.empty:
        last30["net"] = last30.apply(
            lambda r: -r["amount"] if determine_tx_type(r) == "Deposit" else r["amount"],
            axis=1
        )

        daily = last30.groupby(last30["time"].dt.date)["net"].sum()

        fig_bar, ax_bar = plt.subplots(figsize=(8, 3.8))
        ax_bar.bar(daily.index.astype(str), daily.values)
        ax_bar.set_title("Daily Net Flow (Last 30 Days)")
        ax_bar.tick_params(axis='x', rotation=45)

        st.pyplot(fig_bar)
    else:
        st.info("No recent transactions to display.")
# ----------- DEPOSIT ----------
def deposit_page():
    navbar()
    st.title("Deposit")

    accounts = fetch_accounts_cached(st.session_state.user_id)

    if not accounts:
        st.info("No accounts found.")
        return

    acc = st.selectbox("Select Account", [a["account_number"] for a in accounts])
    amt = st.number_input("Amount", min_value=1.0)

    if st.button("Deposit", key="deposit_btn"):
        res = api_post("/deposit", {"account_number": acc, "amount": float(amt)})
        if res.get("status") == "ok":
            st.success("Deposit successful")
            fetch_accounts_cached.clear()
            fetch_transactions_cached.clear()
            st.rerun()
        else:
            st.error("Deposit failed: " + str(res))


# ----------- WITHDRAW ----------
def withdraw_page():
    navbar()
    st.title("Withdraw")

    accounts = fetch_accounts_cached(st.session_state.user_id)

    if not accounts:
        st.info("No accounts found.")
        return

    acc = st.selectbox("Select Account", [a["account_number"] for a in accounts])
    amt = st.number_input("Amount", min_value=1.0)

    if st.button("Withdraw", key="withdraw_btn"):
        res = api_post("/withdraw", {"account_number": acc, "amount": float(amt)})
        if res.get("status") == "ok":
            st.success("Withdraw successful")
            fetch_accounts_cached.clear()
            fetch_transactions_cached.clear()
            st.rerun()
        else:
            st.error("Withdraw failed: " + str(res))

# ----------- TRANSFER ----------
def transfer_page():
    navbar()
    st.title("Transfer")

    accounts = fetch_accounts_cached(st.session_state.user_id)
    from_acc = st.selectbox("From Account", [a["account_number"] for a in accounts])
    to_acc = st.text_input("To Account")
    amt = st.number_input("Amount", min_value=1.0)

    if st.button("Send", key="transfer_btn"):
        res = api_post("/transfer", {"from": from_acc, "to": to_acc, "amount": float(amt)})
        if res.get("status") == "ok":
            st.success("Transfer done")
            fetch_accounts_cached.clear()
            fetch_transactions_cached.clear()
            st.rerun()
        else:
            st.error(str(res))

# ----------- HISTORY ----------
def history_page():
    navbar()
    st.title("Transaction History")

    accounts = fetch_accounts_cached(st.session_state.user_id)
    acc = st.selectbox("Account", [a["account_number"] for a in accounts])

    if st.button("Load History", key="load_history_btn"):
        txs = fetch_transactions_cached(acc)
        if txs:
            df = pd.DataFrame(txs)
            df["time"] = pd.to_datetime(df["time"])
            st.dataframe(df)
        else:
            st.info("No transactions")

# ---------------- ROUTER ----------------
def router():
    if st.session_state.page == "login":
        login_page()
    elif st.session_state.page == "signup":
        signup_page()
    elif st.session_state.page == "dashboard":
        dashboard_page()
    elif st.session_state.page == "deposit":
        deposit_page()
    elif st.session_state.page == "withdraw":
        withdraw_page()
    elif st.session_state.page == "transfer":
        transfer_page()
    elif st.session_state.page == "history":
        history_page()
    else:
        login_page()

router()
