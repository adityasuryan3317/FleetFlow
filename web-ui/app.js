/* FleetFlow Web UI — first-stage browser prototype.
 * This UI intentionally runs in browser localStorage. It is not yet connected
 * to the C++ service layer / CSV files; the integration adapter is a later step.
 */
(() => {
  'use strict';

  const STORAGE_KEY = 'fleetflow.web-ui.preview.v1';
  const seedData = {
    vehicles: [
      { id: 1001, type: 'CAR', brand: 'Toyota', model: 'C', plate: 'HR26AB1234', baseRate: 2500, status: 'Available', seats: 5, fuel: 'Petrol', automatic: true }
    ],
    customers: [
      { id: 2001, role: 'CUSTOMER', username: 'demo_customer_2001', fullName: 'Aarav Sharma', email: 'aarav.sharma@example.com', license: 'TEST-DL-2001', phone: '0000000000' }
    ],
    rentals: [
      { id: 1001, customerId: 2001, vehicleId: 1001, startDate: '2026-11-16', endDate: '2026-11-17', actualStart: '2026-11-16', actualReturn: '2026-11-17', estimatedCost: 5030, finalCost: 2500, status: 'Completed' }
    ],
    payments: [
      { id: 5001, rentalId: 1001, customerId: 2001, amount: 2500, date: '2026-11-15', type: 'Payment', method: 'UPI', description: 'Test advance payment' }
    ],
    maintenance: [
      { id: 3001, vehicleId: 1001, problem: 'Regular engine oil change and routine servicing', startDate: '2026-11-20', completionDate: '2026-11-20', cost: 3500, status: 'Completed' }
    ]
  };

  const clone = value => JSON.parse(JSON.stringify(value));
  let state = loadState();
  let activePage = 'overview';
  let activeFilter = 'All';
  let tableQuery = '';
  let toastTimer = null;

  function loadState() {
    try {
      const saved = localStorage.getItem(STORAGE_KEY);
      if (saved) {
        const parsed = JSON.parse(saved);
        if (parsed && Array.isArray(parsed.vehicles) && Array.isArray(parsed.customers)) return parsed;
      }
    } catch (error) {
      console.warn('FleetFlow preview data could not be loaded; using sample data.', error);
    }
    return clone(seedData);
  }

  function persist() {
    try {
      localStorage.setItem(STORAGE_KEY, JSON.stringify(state));
      return true;
    } catch (error) {
      console.error('Could not save browser preview data.', error);
      toast('Browser storage is unavailable. Your latest change may not persist.', 'error');
      return false;
    }
  }

  const $ = (selector, root = document) => root.querySelector(selector);
  const $$ = (selector, root = document) => [...root.querySelectorAll(selector)];
  const money = value => new Intl.NumberFormat('en-IN', { style: 'currency', currency: 'INR', maximumFractionDigits: 0 }).format(Number(value || 0));
  const formatDate = value => value ? new Date(`${value}T00:00:00`).toLocaleDateString('en-IN', { day: '2-digit', month: 'short', year: 'numeric' }) : '—';
  const todayISO = () => {
    const now = new Date();
    const local = new Date(now.getTime() - now.getTimezoneOffset() * 60000);
    return local.toISOString().slice(0, 10);
  };
  const todayLong = () => new Intl.DateTimeFormat('en-IN', { weekday: 'long', day: 'numeric', month: 'long', year: 'numeric' }).format(new Date()).toUpperCase();
  const todayCompact = () => new Intl.DateTimeFormat('en-IN', { weekday: 'short', day: '2-digit', month: 'short', year: 'numeric' }).format(new Date()).toUpperCase();
  const greeting = () => { const hour = new Date().getHours(); return hour < 12 ? 'Good morning' : hour < 17 ? 'Good afternoon' : 'Good evening'; };
  const escapeHtml = value => String(value ?? '').replace(/[&<>"']/g, char => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' })[char]);
  const getCustomer = id => state.customers.find(item => Number(item.id) === Number(id));
  const getVehicle = id => state.vehicles.find(item => Number(item.id) === Number(id));
  const getRental = id => state.rentals.find(item => Number(item.id) === Number(id));
  const effectiveRate = vehicle => {
    if (!vehicle) return 0;
    let rate = Number(vehicle.baseRate || 0);
    if (vehicle.type === 'CAR') {
      if (vehicle.automatic) rate += 15;
      if (Number(vehicle.seats || 0) > 5) rate += 20;
    }
    if (vehicle.type === 'BIKE') {
      if (Number(vehicle.engineCC || 0) > 350) rate += 15;
      if (!vehicle.hasGear) rate = Math.max(5, rate - 5);
    }
    if (vehicle.type === 'TRUCK') {
      rate += Number(vehicle.capacityTons || 0) * 10;
      rate += Math.max(0, Number(vehicle.axles || 2) - 2) * 25;
    }
    return rate;
  };
  const daysBetweenInclusive = (from, to) => {
    const a = new Date(`${from}T00:00:00`);
    const b = new Date(`${to}T00:00:00`);
    return Math.floor((b - a) / 86400000) + 1;
  };
  const netForRental = id => state.payments.filter(p => Number(p.rentalId) === Number(id)).reduce((sum, p) => sum + (p.type === 'Refund' ? -1 : 1) * Number(p.amount), 0);
  const initials = value => String(value || '?').trim().split(/\s+/).slice(0, 2).map(part => part[0]?.toUpperCase() || '').join('');
  const positiveInteger = (value, label) => { const number = Number(value); if (!Number.isSafeInteger(number) || number <= 0) throw new Error(`${label} must be a positive whole number.`); return number; };
  const finiteAmount = (value, label, allowZero = false) => { const number = Number(value); if (!Number.isFinite(number) || (allowZero ? number < 0 : number <= 0)) throw new Error(`${label} must be ${allowZero ? 'zero or greater' : 'greater than zero'}.`); return number; };
  const titleCase = value => String(value || '').replace(/([a-z])([A-Z])/g, '$1 $2').replace(/^./, c => c.toUpperCase());

  function statusBadge(status) {
    const key = String(status || '').toLowerCase().replace(/\s+/g, '');
    return `<span class="status ${escapeHtml(key)}">${escapeHtml(titleCase(status))}</span>`;
  }

  function heading(eyebrow, title, description, actions = '') {
    return `<div class="page-heading"><div><div class="eyebrow">${eyebrow}</div><h1>${title}</h1><p>${description}</p></div><div class="heading-actions">${actions}</div></div>`;
  }

  function updateShell() {
    const pageNames = { overview: 'Overview', fleet: 'Fleet', rentals: 'Rentals', customers: 'Customers', payments: 'Payments', maintenance: 'Maintenance', reports: 'Reports' };
    $('#breadcrumbCurrent').textContent = pageNames[activePage] || 'Overview';
    $$('.nav-link').forEach(button => button.classList.toggle('active', button.dataset.page === activePage));
    $('#fleetNavCount').textContent = state.vehicles.length;
  }

  function render() {
    updateShell();
    const pages = {
      overview: renderOverview,
      fleet: renderFleet,
      rentals: renderRentals,
      customers: renderCustomers,
      payments: renderPayments,
      maintenance: renderMaintenance,
      reports: renderReports
    };
    $('#mainView').innerHTML = (pages[activePage] || renderOverview)();
  }

  function metric(label, value, note, icon, tone) {
    return `<article class="metric-card"><div class="metric-top"><span class="metric-label">${label}</span><span class="metric-icon ${tone}">${icon}</span></div><div class="metric-value">${value}</div><div class="metric-foot">${note}</div></article>`;
  }

  function renderOverview() {
    const available = state.vehicles.filter(v => v.status === 'Available').length;
    const active = state.rentals.filter(r => r.status === 'Active').length;
    const inService = state.vehicles.filter(v => v.status === 'UnderMaintenance').length;
    const net = state.payments.reduce((sum, p) => sum + (p.type === 'Refund' ? -1 : 1) * Number(p.amount), 0);
    const utilization = state.vehicles.length ? Math.round((state.vehicles.length - available) / state.vehicles.length * 100) : 0;
    const recent = [
      ...state.rentals.map(r => ({ date: r.actualReturn || r.actualStart || r.startDate, title: `Rental #${r.id}`, detail: `${getCustomer(r.customerId)?.fullName || `Customer #${r.customerId}`} · ${getVehicle(r.vehicleId)?.brand || 'Vehicle'} ${getVehicle(r.vehicleId)?.model || ''}`, amount: r.finalCost ?? r.estimatedCost, status: r.status, kind: 'rental' })),
      ...state.payments.map(p => ({ date: p.date, title: `Transaction #${p.id}`, detail: `${p.method} · ${p.type} · Rental #${p.rentalId}`, amount: p.amount, status: p.type, kind: 'payment' })),
      ...state.maintenance.map(m => ({ date: m.completionDate || m.startDate, title: `Maintenance #${m.id}`, detail: `Vehicle #${m.vehicleId} · ${m.problem}`, amount: m.cost, status: m.status, kind: 'maintenance' }))
    ].sort((a, b) => String(b.date).localeCompare(String(a.date))).slice(0, 5);

    return `${heading(todayLong(), `${greeting()}, Aditya <span class="heading-spark">✳</span>`, 'Here’s what’s happening across your fleet today.', `<button class="btn" data-action="export"><span class="btn-icon">⇩</span> Export</button><button class="btn btn-primary" data-action="new-rental"><span class="btn-icon">＋</span> New rental</button>`)}
      <section class="welcome-strip"><div class="welcome-copy"><div class="eyebrow">YOUR OPERATIONS, IN ONE VIEW</div><h2>Keep every journey moving.</h2><p>Track vehicles, manage bookings and stay on top of day-to-day operations.</p></div><div class="welcome-meta"><div class="welcome-date">${todayCompact()}</div></div><div class="welcome-art"></div></section>
      <section class="metrics-grid">
        ${metric('Total fleet', state.vehicles.length, '<strong>Registered vehicles</strong>', '▰', 'green')}
        ${metric('Available vehicles', available, `${state.vehicles.length ? Math.round(available / state.vehicles.length * 100) : 0}% of fleet ready`, '✓', 'blue')}
        ${metric('Active rentals', active, 'Vehicles currently checked out', '▣', 'orange')}
        ${metric('Net recorded payments', money(net), `${state.payments.length} ledger transaction${state.payments.length === 1 ? '' : 's'}`, '◈', 'purple')}
      </section>
      <section class="dashboard-grid">
        <article class="panel"><div class="panel-header"><div><div class="panel-title">Fleet availability</div><div class="panel-subtitle">Operational status across registered vehicles</div></div><button class="text-link" data-page="fleet">View fleet →</button></div><div class="panel-body"><div class="utilization"><div class="donut-wrap"><div class="donut" style="background:conic-gradient(#27875f 0 ${available ? (available / Math.max(1, state.vehicles.length) * 100) : 0}%, #dce7e2 ${available ? (available / Math.max(1, state.vehicles.length) * 100) : 0}% 100%)"></div><div class="donut-center"><strong>${utilization}%</strong><span>In use / service</span></div></div><div class="legend-list"><div class="legend-row"><span class="legend-name"><i class="legend-dot" style="background:#27875f"></i>Available</span><strong>${available}</strong></div><div class="legend-row"><span class="legend-name"><i class="legend-dot" style="background:#6f82d5"></i>Active rentals</span><strong>${active}</strong></div><div class="legend-row"><span class="legend-name"><i class="legend-dot" style="background:#dfa05c"></i>Under maintenance</span><strong>${inService}</strong></div><div class="legend-note">Status is based on the current operational and rental records in this preview.</div></div></div></div></article>
        <article class="panel"><div class="panel-header"><div><div class="panel-title">Quick actions</div><div class="panel-subtitle">Common tasks, one click away</div></div></div><div class="panel-body"><div class="quick-actions"><button class="quick-action" data-action="new-vehicle"><span class="qa-icon">＋</span><strong>Add vehicle</strong><span>Register a new asset</span></button><button class="quick-action" data-action="new-customer"><span class="qa-icon">♙</span><strong>Add customer</strong><span>Grow your directory</span></button><button class="quick-action" data-action="new-rental"><span class="qa-icon">▣</span><strong>Book rental</strong><span>Create a reservation</span></button><button class="quick-action" data-action="new-payment"><span class="qa-icon">◈</span><strong>Record payment</strong><span>Update the ledger</span></button></div></div></article>
      </section>
      <section class="panel activity-panel"><div class="panel-header"><div><div class="panel-title">Latest activity</div><div class="panel-subtitle">A quick look at the latest saved records</div></div><button class="text-link" data-page="reports">View insights →</button></div><div class="table-wrap">${recent.length ? `<table class="data-table"><thead><tr><th>Record</th><th>Details</th><th>Date</th><th>Status</th><th>Amount</th></tr></thead><tbody>${recent.map(item => `<tr><td class="primary-cell">${escapeHtml(item.title)}</td><td>${escapeHtml(item.detail)}</td><td>${formatDate(item.date)}</td><td>${statusBadge(item.status)}</td><td class="primary-cell">${money(item.amount)}</td></tr>`).join('')}</tbody></table>` : emptyState('No activity yet', 'Create a vehicle, customer or booking to get started.')}</div></section>`;
  }

  function entityVehicle(v) {
    const type = String(v.type || 'CAR').toLowerCase();
    const symbol = type === 'bike' ? '◉' : type === 'truck' ? '▰' : '▱';
    return `<div class="entity-cell"><div class="entity-thumb ${type}">${symbol}</div><div><div class="primary-cell">${escapeHtml(v.brand)} ${escapeHtml(v.model)}</div><div class="secondary-cell">${escapeHtml(v.plate)} · #${v.id}</div></div></div>`;
  }
  function entityCustomer(c) {
    return `<div class="entity-cell"><div class="avatar avatar-indigo">${escapeHtml(initials(c.fullName))}</div><div><div class="primary-cell">${escapeHtml(c.fullName)}</div><div class="secondary-cell">${escapeHtml(c.email)}</div></div></div>`;
  }
  function rowAction(label, action, id, entity) {
    return `<button class="btn btn-sm btn-quiet" data-action="${action}" data-entity="${entity}" data-id="${id}">${label}</button>`;
  }
  function emptyState(title, description) {
    return `<div class="empty-state"><div style="font-size:25px">⌁</div><strong>${title}</strong><p>${description}</p></div>`;
  }
  function matchesSearch(values) {
    const query = tableQuery.trim().toLowerCase();
    if (!query) return true;
    return values.map(value => String(value ?? '')).join(' ').toLowerCase().includes(query);
  }
  function toolbar(filters = []) {
    return `<div class="table-toolbar"><div class="filter-row">${filters.map(filter => `<button class="filter-chip ${activeFilter === filter ? 'active' : ''}" data-filter="${escapeHtml(filter)}">${escapeHtml(filter)}</button>`).join('')}</div><label class="table-search"><span>⌕</span><input id="tableSearch" type="search" value="${escapeHtml(tableQuery)}" placeholder="Filter this list..."></label></div>`;
  }

  function renderFleet() {
    const filters = ['All', 'Available', 'UnderMaintenance', 'Retired'];
    const rows = state.vehicles.filter(v => (activeFilter === 'All' || v.status === activeFilter) && matchesSearch([v.id, v.type, v.brand, v.model, v.plate, v.status]));
    const available = state.vehicles.filter(v => v.status === 'Available').length;
    const actions = `<button class="btn btn-primary" data-action="new-vehicle"><span class="btn-icon">＋</span> Add vehicle</button>`;
    return `${heading('ASSET MANAGEMENT', 'Fleet', 'Manage your vehicles, specifications and operational readiness.', actions)}
      <div class="summary-strip"><div class="summary-mini"><span>Total vehicles</span><strong>${state.vehicles.length}</strong></div><div class="summary-mini"><span>Available</span><strong>${available}</strong></div><div class="summary-mini"><span>Under maintenance</span><strong>${state.vehicles.filter(v => v.status === 'UnderMaintenance').length}</strong></div><div class="summary-mini"><span>Retired</span><strong>${state.vehicles.filter(v => v.status === 'Retired').length}</strong></div></div>
      ${toolbar(filters)}<section class="panel"><div class="table-wrap">${rows.length ? `<table class="data-table"><thead><tr><th>Vehicle</th><th>Category</th><th>Daily rate</th><th>Specifications</th><th>Status</th><th>Action</th></tr></thead><tbody>${rows.map(v => `<tr><td>${entityVehicle(v)}</td><td>${titleCase(v.type)}</td><td class="primary-cell">${money(effectiveRate(v))}<div class="secondary-cell">Base ${money(v.baseRate)}</div></td><td>${v.type === 'CAR' ? `${v.seats || '—'} seats · ${escapeHtml(v.fuel || '—')} · ${v.automatic ? 'Automatic' : 'Manual'}` : v.type === 'BIKE' ? `${v.engineCC || '—'} cc · ${v.hasGear ? 'Geared' : 'Gearless'}` : `${v.capacityTons || '—'} tons · ${v.axles || '—'} axles`}</td><td>${statusBadge(v.status)}</td><td><button class="table-action" data-action="vehicle-status" data-id="${v.id}" aria-label="Change vehicle status">•••</button></td></tr>`).join('')}</tbody></table>` : emptyState('No vehicles found', 'Adjust your filters or add a vehicle to the fleet.')}</div></section>`;
  }

  function renderRentals() {
    const filters = ['All', 'Confirmed', 'Active', 'Completed', 'Cancelled'];
    const rows = state.rentals.filter(r => (activeFilter === 'All' || r.status === activeFilter) && matchesSearch([r.id, r.customerId, getCustomer(r.customerId)?.fullName, r.vehicleId, r.status]));
    return `${heading('BOOKING MANAGEMENT', 'Rentals', 'Create reservations and follow each booking through its lifecycle.', `<button class="btn btn-primary" data-action="new-rental"><span class="btn-icon">＋</span> New rental</button>`)}
      <div class="summary-strip"><div class="summary-mini"><span>Total bookings</span><strong>${state.rentals.length}</strong></div><div class="summary-mini"><span>Confirmed</span><strong>${state.rentals.filter(r => r.status === 'Confirmed').length}</strong></div><div class="summary-mini"><span>Active</span><strong>${state.rentals.filter(r => r.status === 'Active').length}</strong></div><div class="summary-mini"><span>Completed</span><strong>${state.rentals.filter(r => r.status === 'Completed').length}</strong></div></div>
      ${toolbar(filters)}<section class="panel"><div class="table-wrap">${rows.length ? `<table class="data-table"><thead><tr><th>Rental</th><th>Customer</th><th>Vehicle</th><th>Scheduled period</th><th>Estimated / final</th><th>Status</th><th>Next step</th></tr></thead><tbody>${rows.map(r => { const c = getCustomer(r.customerId), v = getVehicle(r.vehicleId); const next = r.status === 'Confirmed' ? `${rowAction('Activate', 'activate-rental', r.id, 'rental')} ${rowAction('Cancel', 'cancel-rental', r.id, 'rental')}` : r.status === 'Active' ? rowAction('Complete', 'complete-rental', r.id, 'rental') : '<span class="secondary-cell">—</span>'; return `<tr><td class="primary-cell">#${r.id}<div class="secondary-cell">Customer #${r.customerId}</div></td><td>${c ? entityCustomer(c) : `#${r.customerId}`}</td><td>${v ? `${escapeHtml(v.brand)} ${escapeHtml(v.model)}` : `#${r.vehicleId}`}<div class="secondary-cell">${escapeHtml(v?.plate || '')}</div></td><td>${formatDate(r.startDate)}<div class="secondary-cell">to ${formatDate(r.endDate)}</div></td><td class="primary-cell">${money(r.finalCost ?? r.estimatedCost)}<div class="secondary-cell">${r.finalCost != null ? 'Final settled' : 'Estimate'}</div></td><td>${statusBadge(r.status)}</td><td>${next}</td></tr>`; }).join('')}</tbody></table>` : emptyState('No rentals found', 'Bookings will appear here when you create a reservation.')}</div></section>`;
  }

  function renderCustomers() {
    const rows = state.customers.filter(c => matchesSearch([c.id, c.fullName, c.username, c.email, c.phone, c.license]));
    return `${heading('PEOPLE DIRECTORY', 'Customers', 'Keep customer contact and identification details organized.', `<button class="btn btn-primary" data-action="new-customer"><span class="btn-icon">＋</span> Add customer</button>`)}
      <div class="summary-strip"><div class="summary-mini"><span>Registered customers</span><strong>${state.customers.length}</strong></div><div class="summary-mini"><span>Customer rentals</span><strong>${state.rentals.length}</strong></div></div>
      ${toolbar([])}<section class="panel"><div class="table-wrap">${rows.length ? `<table class="data-table"><thead><tr><th>Customer</th><th>Customer ID</th><th>Phone</th><th>Driving licence</th><th>Bookings</th><th>Net paid</th></tr></thead><tbody>${rows.map(c => `<tr><td>${entityCustomer(c)}</td><td class="primary-cell">#${c.id}<div class="secondary-cell">${escapeHtml(c.username)}</div></td><td>${escapeHtml(c.phone || '—')}</td><td>${escapeHtml(c.license || '—')}</td><td>${state.rentals.filter(r => Number(r.customerId) === Number(c.id)).length}</td><td class="primary-cell">${money(state.payments.filter(p => Number(p.customerId) === Number(c.id)).reduce((sum, p) => sum + (p.type === 'Refund' ? -1 : 1) * Number(p.amount), 0))}</td></tr>`).join('')}</tbody></table>` : emptyState('No customers found', 'Add a customer to start building your directory.')}</div></section>`;
  }

  function renderPayments() {
    const filters = ['All', 'Payment', 'Refund'];
    const rows = state.payments.filter(p => (activeFilter === 'All' || p.type === activeFilter) && matchesSearch([p.id, p.rentalId, p.customerId, p.method, p.type, p.description]));
    const inflow = state.payments.filter(p => p.type === 'Payment').reduce((s, p) => s + Number(p.amount), 0);
    const refunds = state.payments.filter(p => p.type === 'Refund').reduce((s, p) => s + Number(p.amount), 0);
    return `${heading('FINANCIAL LEDGER', 'Payments', 'Track incoming payments and refunds as an immutable transaction ledger.', `<button class="btn btn-primary" data-action="new-payment"><span class="btn-icon">＋</span> Record transaction</button>`)}
      <div class="summary-strip"><div class="summary-mini"><span>Payment inflow</span><strong>${money(inflow)}</strong></div><div class="summary-mini"><span>Refunds</span><strong>${money(refunds)}</strong></div><div class="summary-mini"><span>Net recorded</span><strong>${money(inflow - refunds)}</strong></div><div class="summary-mini"><span>Transactions</span><strong>${state.payments.length}</strong></div></div>
      ${toolbar(filters)}<section class="panel"><div class="table-wrap">${rows.length ? `<table class="data-table"><thead><tr><th>Transaction</th><th>Rental / customer</th><th>Date</th><th>Method</th><th>Description</th><th>Amount</th><th>Type</th></tr></thead><tbody>${rows.map(p => `<tr><td class="primary-cell">#${p.id}</td><td>Rental #${p.rentalId}<div class="secondary-cell">Customer #${p.customerId}</div></td><td>${formatDate(p.date)}</td><td>${escapeHtml(p.method)}</td><td>${escapeHtml(p.description)}</td><td class="primary-cell">${p.type === 'Refund' ? '−' : '+'}${money(p.amount)}</td><td>${statusBadge(p.type)}</td></tr>`).join('')}</tbody></table>` : emptyState('No transactions found', 'Payment and refund entries will appear here.')}</div></section>`;
  }

  function renderMaintenance() {
    const filters = ['All', 'Scheduled', 'InProgress', 'Completed'];
    const rows = state.maintenance.filter(m => (activeFilter === 'All' || m.status === activeFilter) && matchesSearch([m.id, m.vehicleId, m.problem, m.status]));
    const cost = state.maintenance.reduce((sum, m) => sum + Number(m.cost || 0), 0);
    return `${heading('WORKSHOP MANAGEMENT', 'Maintenance', 'Schedule service, track workshop progress and record completion costs.', `<button class="btn btn-primary" data-action="new-maintenance"><span class="btn-icon">＋</span> Schedule service</button>`)}
      <div class="summary-strip"><div class="summary-mini"><span>Service records</span><strong>${state.maintenance.length}</strong></div><div class="summary-mini"><span>Scheduled</span><strong>${state.maintenance.filter(m => m.status === 'Scheduled').length}</strong></div><div class="summary-mini"><span>In progress</span><strong>${state.maintenance.filter(m => m.status === 'InProgress').length}</strong></div><div class="summary-mini"><span>Recorded service cost</span><strong>${money(cost)}</strong></div></div>
      ${toolbar(filters)}<section class="panel"><div class="table-wrap">${rows.length ? `<table class="data-table"><thead><tr><th>Service record</th><th>Vehicle</th><th>Issue / service</th><th>Start date</th><th>Completion</th><th>Cost</th><th>Status</th><th>Next step</th></tr></thead><tbody>${rows.map(m => { const v = getVehicle(m.vehicleId); const next = m.status === 'Scheduled' ? rowAction('Start', 'start-maintenance', m.id, 'maintenance') : m.status === 'InProgress' ? rowAction('Complete', 'complete-maintenance', m.id, 'maintenance') : '<span class="secondary-cell">—</span>'; return `<tr><td class="primary-cell">#${m.id}</td><td>${v ? `${escapeHtml(v.brand)} ${escapeHtml(v.model)}` : `#${m.vehicleId}`}<div class="secondary-cell">Vehicle #${m.vehicleId}</div></td><td>${escapeHtml(m.problem)}</td><td>${formatDate(m.startDate)}</td><td>${formatDate(m.completionDate)}</td><td class="primary-cell">${money(m.cost)}</td><td>${statusBadge(m.status)}</td><td>${next}</td></tr>`; }).join('')}</tbody></table>` : emptyState('No maintenance records found', 'Schedule a service record to track workshop activity.')}</div></section>`;
  }

  function renderReports() {
    const paymentTotal = state.payments.filter(p => p.type === 'Payment').reduce((s, p) => s + Number(p.amount), 0);
    const refundTotal = state.payments.filter(p => p.type === 'Refund').reduce((s, p) => s + Number(p.amount), 0);
    const settled = state.rentals.filter(r => r.status === 'Completed').reduce((s, r) => s + Number(r.finalCost || 0), 0);
    const serviceCost = state.maintenance.reduce((s, m) => s + Number(m.cost || 0), 0);
    const counts = [
      ['Vehicles', state.vehicles.length], ['Customers', state.customers.length], ['Rentals', state.rentals.length], ['Payment transactions', state.payments.length], ['Maintenance records', state.maintenance.length]
    ];
    return `${heading('BUSINESS SNAPSHOT', 'Reports & insights', 'A concise view of the records currently held in this browser preview.', `<button class="btn" data-action="export"><span class="btn-icon">⇩</span> Export summary</button>`)}
      <div class="report-grid"><article class="panel report-card"><h3>Record overview</h3><p>Current data volume by module</p><div style="margin-top:14px">${counts.map(([label, value]) => `<div class="report-stat"><span>${label}</span><strong>${value}</strong></div>`).join('')}</div></article>
      <article class="panel report-card"><h3>Financial snapshot</h3><p>Calculated from the local payment and rental ledgers</p><div style="margin-top:14px"><div class="report-stat"><span>Payment inflow</span><strong>${money(paymentTotal)}</strong></div><div class="report-stat"><span>Refund outflow</span><strong>${money(refundTotal)}</strong></div><div class="report-stat"><span>Net recorded transactions</span><strong>${money(paymentTotal - refundTotal)}</strong></div><div class="report-stat"><span>Completed rental final costs</span><strong>${money(settled)}</strong></div><div class="report-stat"><span>Maintenance costs</span><strong>${money(serviceCost)}</strong></div></div></article></div>
      <article class="panel report-card" style="margin-top:15px"><h3>About these figures</h3><p>These values are computed from the browser preview's current records. They are not live analytics from the C++ application or a production database. For final integration, the same views will consume data through a local service/API adapter.</p></article>`;
  }

  function toast(message, kind = 'success') {
    const region = $('#toastRegion');
    const item = document.createElement('div');
    item.className = 'toast';
    item.innerHTML = `<i>${kind === 'error' ? '!' : '✓'}</i><span>${escapeHtml(message)}</span>`;
    region.appendChild(item);
    setTimeout(() => item.remove(), 3400);
  }

  function openModal(title, eyebrow, fields, submitLabel, handler) {
    $('#modalTitle').textContent = title;
    $('#modalEyebrow').textContent = eyebrow;
    $('#modalBody').innerHTML = `<form class="modal-form" id="recordForm" novalidate><div class="form-grid">${fields}</div><div class="form-footer"><button class="btn" type="button" data-close-modal>Cancel</button><button class="btn btn-primary" type="submit">${submitLabel}</button></div></form>`;
    const modal = $('#appModal');
    if (typeof modal.showModal === 'function') modal.showModal(); else modal.setAttribute('open', 'open');
    $('#recordForm').addEventListener('submit', event => {
      event.preventDefault();
      const form = new FormData(event.currentTarget);
      const values = Object.fromEntries(form.entries());
      try {
        const missingField = [...event.currentTarget.querySelectorAll('[required]')].find(control => !String(control.value || '').trim());
        if (missingField) throw new Error('Please complete all required fields.');
        const message = handler(values);
        persist();
        closeModal();
        render();
        if (message) toast(message);
      } catch (error) {
        toast(error.message || 'Please review the form and try again.', 'error');
      }
    });
    $$('[data-close-modal]').forEach(button => button.addEventListener('click', closeModal));
  }

  function closeModal() {
    const modal = $('#appModal');
    if (typeof modal.close === 'function' && modal.open) modal.close(); else modal.removeAttribute('open');
  }
  function field(label, name, type = 'text', value = '', options = {}) {
    const required = options.required === false ? '' : 'required';
    const cls = options.full ? 'field full' : 'field';
    let control = '';
    if (type === 'select') {
      control = `<select name="${name}" ${required}>${(options.choices || []).map(choice => `<option value="${escapeHtml(choice.value)}" ${String(choice.value) === String(value) ? 'selected' : ''}>${escapeHtml(choice.label)}</option>`).join('')}</select>`;
    } else if (type === 'textarea') {
      control = `<textarea name="${name}" ${required} placeholder="${escapeHtml(options.placeholder || '')}">${escapeHtml(value)}</textarea>`;
    } else {
      control = `<input name="${name}" type="${type}" value="${escapeHtml(value)}" ${required} ${options.min != null ? `min="${options.min}"` : ''} ${options.step ? `step="${options.step}"` : ''} placeholder="${escapeHtml(options.placeholder || '')}">`;
    }
    return `<div class="${cls}"><label>${label}</label>${control}${options.hint ? `<span class="field-hint">${options.hint}</span>` : ''}</div>`;
  }

  function openVehicleModal() {
    openModal('Add vehicle', 'FLEET MANAGEMENT',
      field('Vehicle ID', 'id', 'number') + field('Vehicle type', 'type', 'select', 'CAR', { choices: [{ value: 'CAR', label: 'Car' }, { value: 'BIKE', label: 'Bike' }, { value: 'TRUCK', label: 'Truck' }] }) +
      field('Brand', 'brand') + field('Model', 'model') + field('License plate', 'plate') + field('Base daily rate (₹)', 'baseRate', 'number', '', { min: '0.01', step: '0.01' }) +
      field('Operational status', 'status', 'select', 'Available', { choices: ['Available', 'UnderMaintenance', 'Retired'].map(x => ({ value: x, label: titleCase(x) })) }) +
      field('Seats (cars)', 'seats', 'number', '5', { min: 1, required: false }) + field('Fuel type (cars)', 'fuel', 'text', 'Petrol', { required: false }) +
      field('Automatic? (cars)', 'automatic', 'select', 'true', { required: false, choices: [{ value: 'true', label: 'Automatic' }, { value: 'false', label: 'Manual' }] }) +
      field('Engine cc (bikes)', 'engineCC', 'number', '150', { min: 1, required: false }) + field('Has gears? (bikes)', 'hasGear', 'select', 'true', { required: false, choices: [{ value: 'true', label: 'Geared' }, { value: 'false', label: 'Gearless' }] }) +
      field('Cargo capacity (tons)', 'capacityTons', 'number', '2.5', { min: 0, step: '.1', required: false }) + field('Number of axles', 'axles', 'number', '2', { min: 2, required: false }), 'Save vehicle', values => {
        const id = positiveInteger(values.id, 'Vehicle ID');
        const baseRate = finiteAmount(values.baseRate, 'Base daily rate');
        if (state.vehicles.some(v => Number(v.id) === id)) throw new Error('A vehicle with this ID already exists.');
        if (state.vehicles.some(v => v.plate.toLowerCase() === values.plate.toLowerCase())) throw new Error('This license plate is already registered.');
        if (values.type === 'CAR' && (!Number.isInteger(Number(values.seats)) || Number(values.seats) < 1 || Number(values.seats) > 9 || !String(values.fuel || '').trim())) throw new Error('Cars need 1–9 seats and a fuel type.');
        if (values.type === 'BIKE' && positiveInteger(values.engineCC, 'Engine capacity') <= 0) throw new Error('Engine capacity must be positive.');
        if (values.type === 'TRUCK' && (!(Number(values.capacityTons) > 0) || !Number.isFinite(Number(values.capacityTons)) || Number(values.axles) < 2 || !Number.isInteger(Number(values.axles)))) throw new Error('Trucks need positive cargo capacity and at least two axles.');
        state.vehicles.push({ id, type: values.type, brand: values.brand.trim(), model: values.model.trim(), plate: values.plate.trim().toUpperCase(), baseRate, status: values.status, seats: Number(values.seats || 5), fuel: values.fuel || 'Petrol', automatic: values.automatic !== 'false', engineCC: Number(values.engineCC || 150), hasGear: values.hasGear !== 'false', capacityTons: Number(values.capacityTons || 2.5), axles: Number(values.axles || 2) });
        return `Vehicle #${values.id} added to the preview.`;
      });
  }

  function openCustomerModal() {
    openModal('Add customer', 'CUSTOMER DIRECTORY', field('Customer ID', 'id', 'number') + field('Full name', 'fullName') + field('Username', 'username') + field('Email address', 'email', 'email') + field('Driving licence', 'license') + field('Phone number', 'phone'), 'Save customer', values => {
      const id = positiveInteger(values.id, 'Customer ID');
      if (state.customers.some(c => Number(c.id) === id)) throw new Error('A customer with this ID already exists.');
      if (!/^[^\s@]+@[^\s@]+\.[^\s@]+$/.test(values.email)) throw new Error('Enter a valid email address.');
      if (state.customers.some(c => c.email.toLowerCase() === values.email.toLowerCase())) throw new Error('This email is already registered.');
      state.customers.push({ id, role: 'CUSTOMER', fullName: values.fullName.trim(), username: values.username.trim(), email: values.email.trim(), license: values.license.trim(), phone: values.phone.trim() });
      return `Customer ${values.fullName} added to the preview.`;
    });
  }

  function openRentalModal() {
    const customers = state.customers.map(c => ({ value: c.id, label: `${c.fullName} · #${c.id}` }));
    const vehicles = state.vehicles.filter(v => v.status === 'Available').map(v => ({ value: v.id, label: `${v.brand} ${v.model} · ${v.plate}` }));
    if (!customers.length) return toast('Add a customer before creating a rental.', 'error');
    if (!vehicles.length) return toast('No operationally available vehicle is available for a new rental.', 'error');
    const nextId = Math.max(1000, ...state.rentals.map(r => Number(r.id))) + 1;
    openModal('Create rental', 'BOOKING WORKFLOW', field('Rental ID', 'id', 'number', nextId) + field('Customer', 'customerId', 'select', customers[0].value, { choices: customers }) + field('Vehicle', 'vehicleId', 'select', vehicles[0].value, { choices: vehicles }) + field('Scheduled start', 'startDate', 'date', todayISO()) + field('Scheduled end', 'endDate', 'date', todayISO()) + `<div class="field full"><span class="field-hint">The estimate uses the inclusive date range and the vehicle’s effective daily rate. This preview does not call the C++ RentalService yet.</span></div>`, 'Create booking', values => {
      const rentalId = positiveInteger(values.id, 'Rental ID');
      const customerId = positiveInteger(values.customerId, 'Customer ID');
      const vehicleId = positiveInteger(values.vehicleId, 'Vehicle ID');
      if (state.rentals.some(r => Number(r.id) === rentalId)) throw new Error('A rental with this ID already exists.');
      if (!getCustomer(customerId) || !getVehicle(vehicleId)) throw new Error('Select an existing customer and vehicle.');
      if (values.endDate < values.startDate) throw new Error('Scheduled end date cannot be before the start date.');
      const vehicle = getVehicle(vehicleId);
      const days = daysBetweenInclusive(values.startDate, values.endDate);
      const conflicts = state.rentals.some(r => {
        if (Number(r.vehicleId) !== vehicleId) return false;
        if (r.status === 'Cancelled') return false;
        if (r.status === 'Confirmed') return values.startDate <= r.endDate && values.endDate >= r.startDate;
        if (r.status === 'Active') return values.endDate >= (r.actualStart || r.startDate);
        if (r.status === 'Completed') {
          const occupiedFrom = r.actualStart || r.startDate;
          const occupiedThrough = r.actualReturn || r.endDate;
          return values.startDate <= occupiedThrough && values.endDate >= occupiedFrom;
        }
        return false;
      });
      if (conflicts) throw new Error('This vehicle has an overlapping confirmed or active booking in the preview.');
      const estimate = effectiveRate(vehicle) * days;
      state.rentals.push({ id: rentalId, customerId, vehicleId, startDate: values.startDate, endDate: values.endDate, actualStart: '', actualReturn: '', estimatedCost: estimate, finalCost: null, status: 'Confirmed' });
      return `Rental #${values.id} created. Estimated total: ${money(estimate)}.`;
    });
  }

  function openPaymentModal() {
    if (!state.rentals.length) return toast('Create a rental before recording a transaction.', 'error');
    const options = state.rentals.map(r => ({ value: r.id, label: `Rental #${r.id} · ${getCustomer(r.customerId)?.fullName || `Customer ${r.customerId}`}` }));
    const nextId = Math.max(5000, ...state.payments.map(p => Number(p.id))) + 1;
    openModal('Record transaction', 'PAYMENT LEDGER', field('Transaction ID', 'id', 'number', nextId) + field('Rental', 'rentalId', 'select', options[0].value, { choices: options }) + field('Type', 'type', 'select', 'Payment', { choices: [{ value: 'Payment', label: 'Payment' }, { value: 'Refund', label: 'Refund' }] }) + field('Amount (₹)', 'amount', 'number', '', { min: '.01', step: '.01' }) + field('Transaction date', 'date', 'date', todayISO()) + field('Payment method', 'method', 'select', 'UPI', { choices: ['Cash', 'Card', 'UPI'].map(x => ({ value: x, label: x })) }) + field('Description', 'description', 'text', '', { full: true }), 'Save transaction', values => {
      const paymentId = positiveInteger(values.id, 'Transaction ID');
      const amount = finiteAmount(values.amount, 'Transaction amount');
      if (state.payments.some(p => Number(p.id) === paymentId)) throw new Error('A transaction with this ID already exists.');
      const rental = getRental(values.rentalId);
      if (!rental) throw new Error('Select a valid rental.');
      state.payments.push({ id: paymentId, rentalId: Number(rental.id), customerId: Number(rental.customerId), amount, date: values.date, type: values.type, method: values.method, description: values.description.trim() });
      return `${values.type} of ${money(values.amount)} recorded in the preview ledger.`;
    });
  }

  function openMaintenanceModal() {
    if (!state.vehicles.length) return toast('Add a vehicle before scheduling maintenance.', 'error');
    const options = state.vehicles.filter(v => v.status !== 'Retired').map(v => ({ value: v.id, label: `${v.brand} ${v.model} · #${v.id}` }));
    if (!options.length) return toast('No non-retired vehicle is available for maintenance scheduling.', 'error');
    const nextId = Math.max(3000, ...state.maintenance.map(m => Number(m.id))) + 1;
    openModal('Schedule maintenance', 'WORKSHOP MANAGEMENT', field('Maintenance ID', 'id', 'number', nextId) + field('Vehicle', 'vehicleId', 'select', options[0]?.value, { choices: options }) + field('Service / problem', 'problem', 'textarea', '', { full: true }) + field('Scheduled start', 'startDate', 'date', todayISO()) + field('Estimated cost (₹)', 'cost', 'number', '0', { min: 0, step: '.01' }), 'Schedule service', values => {
      const maintenanceId = positiveInteger(values.id, 'Maintenance ID');
      const vehicleId = positiveInteger(values.vehicleId, 'Vehicle ID');
      const cost = finiteAmount(values.cost || 0, 'Estimated maintenance cost', true);
      if (state.maintenance.some(m => Number(m.id) === maintenanceId)) throw new Error('A maintenance record with this ID already exists.');
      if (!getVehicle(vehicleId) || getVehicle(vehicleId).status === 'Retired') throw new Error('Select a non-retired vehicle.');
      state.maintenance.push({ id: maintenanceId, vehicleId, problem: values.problem.trim(), startDate: values.startDate, completionDate: '', cost, status: 'Scheduled' });
      return `Maintenance #${values.id} scheduled in the preview.`;
    });
  }

  function modalForDate(title, label, onSubmit, includeCost = false, initialCost = 0) {
    const fields = field(label, 'date', 'date', todayISO()) + (includeCost ? field('Final settled cost (₹)', 'cost', 'number', initialCost, { min: 0, step: '.01' }) : '');
    openModal(title, 'LIFECYCLE UPDATE', fields, 'Confirm update', values => onSubmit(values));
  }

  function changeRentalStatus(id, action) {
    const rental = getRental(id);
    if (!rental) return;
    if (action === 'activate-rental') {
      modalForDate(`Activate rental #${id}`, 'Actual pickup date', values => {
        if (values.date < rental.startDate) throw new Error('Actual pickup cannot precede the scheduled start date.');
        rental.actualStart = values.date; rental.status = 'Active';
        return `Rental #${id} activated in the preview.`;
      });
    } else if (action === 'complete-rental') {
      modalForDate(`Complete rental #${id}`, 'Actual return date', values => {
        const actualStart = rental.actualStart || rental.startDate;
        if (values.date < actualStart) throw new Error('Return date cannot precede the actual pickup date.');
        rental.actualReturn = values.date; rental.finalCost = finiteAmount(values.cost, 'Final settled cost', true); rental.status = 'Completed';
        return `Rental #${id} marked completed in the preview.`;
      }, true, rental.estimatedCost);
    } else if (action === 'cancel-rental') {
      if (rental.status !== 'Confirmed') return toast('Only a confirmed rental can be cancelled.', 'error');
      if (!confirm(`Cancel rental #${id}?`)) return;
      rental.status = 'Cancelled';
      persist(); render(); toast(`Rental #${id} cancelled in the preview.`);
    }
  }

  function changeMaintenanceStatus(id, action) {
    const record = state.maintenance.find(m => Number(m.id) === Number(id));
    if (!record) return;
    if (action === 'start-maintenance') {
      const vehicle = getVehicle(record.vehicleId);
      if (!vehicle) return toast('The linked vehicle could not be found.', 'error');
      if (vehicle.status === 'Retired') return toast('A retired vehicle cannot enter maintenance.', 'error');
      if (state.rentals.some(r => Number(r.vehicleId) === Number(record.vehicleId) && r.status === 'Active')) {
        return toast('Maintenance cannot start while this vehicle has an active rental.', 'error');
      }
      if (state.maintenance.some(m => Number(m.vehicleId) === Number(record.vehicleId) && Number(m.id) !== Number(record.id) && m.status === 'InProgress')) {
        return toast('This vehicle already has maintenance in progress.', 'error');
      }
      record.status = 'InProgress';
      vehicle.status = 'UnderMaintenance';
      persist(); render(); toast(`Maintenance #${id} started in the preview.`);
    } else {
      openModal(`Complete maintenance #${id}`, 'WORKSHOP UPDATE', field('Completion date', 'date', 'date', todayISO()) + field('Actual cost (₹)', 'cost', 'number', record.cost, { min: 0, step: '.01' }), 'Complete service', values => {
        if (values.date < record.startDate) throw new Error('Completion date cannot precede the scheduled start date.');
        record.completionDate = values.date; record.cost = finiteAmount(values.cost, 'Actual maintenance cost', true); record.status = 'Completed';
        const otherOpen = state.maintenance.some(m => Number(m.vehicleId) === Number(record.vehicleId) && Number(m.id) !== Number(record.id) && m.status === 'InProgress');
        if (!otherOpen) {
          const vehicle = getVehicle(record.vehicleId);
          if (vehicle && vehicle.status === 'UnderMaintenance') vehicle.status = 'Available';
        }
        return `Maintenance #${id} completed in the preview.`;
      });
    }
  }

  function changeVehicleStatus(id) {
    const vehicle = getVehicle(id);
    if (!vehicle) return;
    openModal(`Vehicle #${id} status`, 'FLEET MANAGEMENT', field('Operational status', 'status', 'select', vehicle.status, { choices: ['Available', 'UnderMaintenance', 'Retired'].map(x => ({ value: x, label: titleCase(x) })) }), 'Update status', values => {
      if (values.status === 'Available' && state.maintenance.some(m => Number(m.vehicleId) === Number(id) && m.status === 'InProgress')) throw new Error('Complete in-progress maintenance before making this vehicle available.');
      if (values.status === 'Available' && state.rentals.some(r => Number(r.vehicleId) === Number(id) && r.status === 'Active')) throw new Error('An active rental prevents this vehicle from being marked available.');
      vehicle.status = values.status;
      return `Vehicle #${id} status updated in the preview.`;
    });
  }

  function exportSummary() {
    const lines = [
      ['Module', 'Count'], ['Vehicles', state.vehicles.length], ['Customers', state.customers.length], ['Rentals', state.rentals.length], ['Payments', state.payments.length], ['Maintenance', state.maintenance.length],
      [], ['Payment ID', 'Rental ID', 'Customer ID', 'Date', 'Type', 'Method', 'Amount', 'Description'],
      ...state.payments.map(p => [p.id, p.rentalId, p.customerId, p.date, p.type, p.method, p.amount, p.description])
    ];
    const csv = lines.map(row => row.map(value => `"${String(value ?? '').replace(/"/g, '""')}"`).join(',')).join('\r\n');
    const blob = new Blob([csv], { type: 'text/csv;charset=utf-8' });
    const url = URL.createObjectURL(blob);
    const link = document.createElement('a'); link.href = url; link.download = 'fleetflow-preview-summary.csv'; link.click(); URL.revokeObjectURL(url);
    toast('Preview summary exported as CSV.');
  }

  function resetPreview() {
    if (!confirm('Reset the browser preview to the original FleetFlow sample records? This only resets this browser preview, not your C++ CSV files.')) return;
    state = clone(seedData); persist(); activePage = 'overview'; activeFilter = 'All'; tableQuery = ''; render(); toast('Browser preview reset to sample data.');
  }

  function helpDialog() {
    openModal('Quick guide', 'FLEETFLOW UI PREVIEW', `<div class="field full"><p style="font-size:11px;line-height:1.7;color:#69778b;margin:0">This is the first UI/UX prototype for FleetFlow. Use the sidebar to move between Overview, Fleet, Rentals, Customers, Payments, Maintenance and Reports. Use the primary buttons to add preview records. Changes are saved only in this browser using localStorage and do not modify the C++ CSV files yet. The next development stage is to connect these screens to the C++ services through a local API.</p></div>`, 'Got it', () => '');
  }

  document.addEventListener('click', event => {
    const pageButton = event.target.closest('[data-page]');
    if (pageButton) {
      activePage = pageButton.dataset.page; activeFilter = 'All'; tableQuery = ''; $('#globalSearch').value = ''; render();
      $('#sidebar').classList.remove('open'); $('#mobileOverlay').classList.remove('show');
      return;
    }
    const filterButton = event.target.closest('[data-filter]');
    if (filterButton) { activeFilter = filterButton.dataset.filter; render(); return; }
    const actionButton = event.target.closest('[data-action]');
    if (actionButton) {
      const action = actionButton.dataset.action; const id = actionButton.dataset.id;
      if (action === 'new-vehicle') openVehicleModal();
      else if (action === 'new-customer') openCustomerModal();
      else if (action === 'new-rental') openRentalModal();
      else if (action === 'new-payment') openPaymentModal();
      else if (action === 'new-maintenance') openMaintenanceModal();
      else if (action === 'activate-rental' || action === 'complete-rental') changeRentalStatus(id, action);
      else if (action === 'start-maintenance' || action === 'complete-maintenance') changeMaintenanceStatus(id, action);
      else if (action === 'vehicle-status') changeVehicleStatus(id);
      else if (action === 'export') exportSummary();
      else if (action === 'help') helpDialog();
      else if (action === 'notifications') toast('You’re all caught up in this preview.');
      return;
    }
    if (event.target.matches('[data-close-modal]')) closeModal();
  });

  document.addEventListener('input', event => {
    if (event.target.id === 'tableSearch') {
      const cursor = event.target.selectionStart;
      tableQuery = event.target.value;
      const page = activePage;
      const scrollY = window.scrollY;
      render();
      const input = $('#tableSearch');
      if (input) { input.focus(); input.setSelectionRange(cursor, cursor); }
      window.scrollTo(0, scrollY);
    }
    if (event.target.id === 'globalSearch') {
      tableQuery = event.target.value;
      if (['fleet', 'rentals', 'customers', 'payments', 'maintenance'].includes(activePage)) render();
    }
  });

  $('#globalSearch').addEventListener('keydown', event => {
    if (event.key === 'Enter') {
      const query = event.target.value.trim().toLowerCase();
      if (!query) return;
      const candidates = [
        ['fleet', state.vehicles.some(v => matchesRecord([v.id, v.brand, v.model, v.plate], query))],
        ['rentals', state.rentals.some(r => matchesRecord([r.id, r.customerId, r.vehicleId, r.status], query))],
        ['customers', state.customers.some(c => matchesRecord([c.id, c.fullName, c.email], query))],
        ['payments', state.payments.some(p => matchesRecord([p.id, p.rentalId, p.description], query))],
        ['maintenance', state.maintenance.some(m => matchesRecord([m.id, m.vehicleId, m.problem], query))]
      ];
      const found = candidates.find(([, match]) => match);
      if (found) { activePage = found[0]; activeFilter = 'All'; tableQuery = event.target.value; render(); }
      else toast('No matching record found in the preview.', 'error');
    }
  });

  function matchesRecord(values, query) { return values.map(x => String(x ?? '')).join(' ').toLowerCase().includes(query); }

  $('.global-search').addEventListener('click', () => { if (window.matchMedia('(max-width: 570px)').matches) { $('.global-search').classList.add('open'); $('#globalSearch').focus(); } });
  $('#globalSearch').addEventListener('blur', () => { if (!$('#globalSearch').value.trim()) $('.global-search').classList.remove('open'); });
  $('#mobileMenu').addEventListener('click', () => { $('#sidebar').classList.add('open'); $('#mobileOverlay').classList.add('show'); });
  $('#mobileOverlay').addEventListener('click', () => { $('#sidebar').classList.remove('open'); $('#mobileOverlay').classList.remove('show'); });
  $('#appModal').addEventListener('click', event => { if (event.target === $('#appModal')) closeModal(); });
  document.addEventListener('keydown', event => {
    if ((event.metaKey || event.ctrlKey) && event.key.toLowerCase() === 'k') { event.preventDefault(); $('#globalSearch').focus(); }
    if (event.key === 'Escape') closeModal();
  });

  render();
})();
