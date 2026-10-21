async function loadAnalysis() {
  const response = await fetch('analysis_output.json');
  if (!response.ok) {
    throw new Error('Unable to load analysis_output.json');
  }
  return response.json();
}

function formatTrend(value) {
  const sign = value > 0 ? '+' : '';
  return `${sign}${value.toFixed(1)}%`;
}

function badgeClass(label) {
  return label.toLowerCase().replace(/_/g, '-');
}

function trendClass(value) {
  if (value <= -40) return 'trend-down-strong';
  if (value < 0) return 'trend-down-soft';
  return 'trend-up';
}

function buildSparkline(values) {
  const width = 220;
  const height = 72;
  const padding = 8;
  const min = Math.min(...values);
  const max = Math.max(...values);
  const range = Math.max(max - min, 1);
  const step = values.length > 1 ? (width - padding * 2) / (values.length - 1) : 0;

  const points = values.map((value, index) => {
    const x = padding + index * step;
    const y = height - padding - ((value - min) / range) * (height - padding * 2);
    return `${x},${y}`;
  }).join(' ');

  return `
    <span class="sparkline-label">Weekly login pattern</span>
    <svg class="sparkline" viewBox="0 0 ${width} ${height}" preserveAspectRatio="none" aria-hidden="true">
      <polyline fill="none" stroke="#0f766e" stroke-width="3" stroke-linecap="round" stroke-linejoin="round" points="${points}"></polyline>
    </svg>
  `;
}

function summarize(accounts) {
  return {
    total: accounts.length,
    flagged: accounts.filter((account) => account.should_flag).length,
    highRisk: accounts.filter((account) => account.risk_label === 'HIGH_RISK').length,
    monitor: accounts.filter((account) => account.risk_label === 'MONITOR').length,
    insufficient: accounts.filter((account) => account.risk_label === 'INSUFFICIENT_DATA').length,
    healthy: accounts.filter((account) => account.risk_label === 'HEALTHY').length,
    enterprise: accounts.filter((account) => account.plan_tier === 'Enterprise').length,
    avgTrend: accounts.reduce((total, account) => total + account.trend_pct, 0) / accounts.length
  };
}

function renderSummary(accounts) {
  const summary = summarize(accounts);
  const cards = [
    { label: 'Flagged for outreach', value: summary.flagged },
    { label: 'High-risk accounts', value: summary.highRisk },
    { label: 'Monitor accounts', value: summary.monitor },
    { label: 'Insufficient data', value: summary.insufficient }
  ];

  document.getElementById('summary-cards').innerHTML = cards.map((card) => `
    <article class="metric-card">
      <strong>${card.value}</strong>
      <span>${card.label}</span>
    </article>
  `).join('');

  document.getElementById('summary-note').textContent = `${summary.flagged} accounts merit immediate outreach, while ${summary.monitor} more should stay on the Customer Success watchlist.`;
}

function renderDatasetStory(accounts) {
  const summary = summarize(accounts);
  const strongestDrop = [...accounts].sort((left, right) => left.trend_pct - right.trend_pct)[0];
  const newestAccount = [...accounts].sort((left, right) => left.history_weeks - right.history_weeks)[0];
  const steadiestHealthy = accounts
    .filter((account) => account.risk_label === 'HEALTHY')
    .sort((left, right) => left.volatility_ratio - right.volatility_ratio)[0];

  const cards = [
    {
      title: `${summary.total} accounts reviewed`,
      body: `The dataset is intentionally small, so this solution favors explainable prioritization over a black-box score.`
    },
    {
      title: `${summary.enterprise} enterprise accounts`,
      body: `Higher-value accounts are visible in the same workflow, but each account is judged against its own prior usage baseline.`
    },
    {
      title: `${strongestDrop.account_name}`,
      body: `Shows the sharpest decline at ${formatTrend(strongestDrop.trend_pct)}, making it the clearest intervention candidate.`
    },
    {
      title: `${newestAccount.account_name}`,
      body: `Has only ${newestAccount.history_weeks} weeks of history, so the model explicitly avoids overconfident churn labeling.`
    },
    {
      title: `${steadiestHealthy.account_name}`,
      body: `Represents stable usage with low volatility, which helps illustrate what a healthy account looks like in this portfolio.`
    },
    {
      title: `${formatTrend(summary.avgTrend)} average trend`,
      body: `Portfolio-wide movement is slightly negative, but only sustained and material declines are escalated for outreach.`
    }
  ];

  document.getElementById('dataset-story').innerHTML = cards.map((card) => `
    <article class="insight-card">
      <strong>${card.title}</strong>
      <p>${card.body}</p>
    </article>
  `).join('');
}

function renderDecisionSteps() {
  const steps = [
    'Start with weekly login history for each account and separate very new accounts from mature ones.',
    'Compare the most recent 4 weeks against the previous 4 weeks to measure direction and magnitude of change.',
    'Check whether the weakness is sustained across multiple recent weeks instead of reacting to a single dip.',
    'Escalate only the accounts with material, persistent decline and keep weaker signals in a monitor queue.'
  ];

  document.getElementById('decision-steps').innerHTML = steps.map((step, index) => `
    <article class="step-card">
      <span class="step-index">${index + 1}</span>
      <p>${step}</p>
    </article>
  `).join('');
}

function renderPortfolioStrip(accounts) {
  const summary = summarize(accounts);
  const cards = [
    {
      title: `${summary.highRisk}`,
      subtitle: 'High-risk accounts',
      note: 'These are the strongest outreach candidates under the current thresholds.'
    },
    {
      title: `${summary.monitor}`,
      subtitle: 'Monitor accounts',
      note: 'These show some weakness, but not enough evidence for immediate escalation.'
    },
    {
      title: `${summary.healthy}`,
      subtitle: 'Healthy accounts',
      note: 'Stable or improving usage patterns with no current churn signal.'
    },
    {
      title: `${summary.insufficient}`,
      subtitle: 'New or thin-history accounts',
      note: 'Handled separately so the model stays honest about uncertainty.'
    }
  ];

  document.getElementById('portfolio-strip').innerHTML = cards.map((card) => `
    <article class="strip-card">
      <strong>${card.title}</strong>
      <span>${card.subtitle}</span>
      <small>${card.note}</small>
    </article>
  `).join('');
}

function renderHeroOrbit(accounts) {
  const summary = summarize(accounts);
  const highestRisk = [...accounts].sort((left, right) => left.trend_pct - right.trend_pct)[0];
  const strongestHealthy = accounts
    .filter((account) => account.risk_label === 'HEALTHY')
    .sort((left, right) => right.trend_pct - left.trend_pct)[0];

  const cards = [
    {
      value: `${summary.flagged}`,
      label: 'Accounts need immediate outreach.'
    },
    {
      value: `${formatTrend(summary.avgTrend)}`,
      label: 'Average portfolio movement across all eight accounts.'
    },
    {
      value: highestRisk.account_id,
      label: `${highestRisk.account_name} shows the sharpest decline.`
    },
    {
      value: strongestHealthy ? strongestHealthy.account_id : 'N/A',
      label: strongestHealthy ? `${strongestHealthy.account_name} remains the healthiest recovery benchmark.` : 'No healthy benchmark available.'
    }
  ];

  document.getElementById('hero-orbit').innerHTML = cards.map((card) => `
    <article class="orbit-card reveal">
      <strong>${card.value}</strong>
      <span>${card.label}</span>
    </article>
  `).join('');
}

function renderFeaturedRisk(accounts) {
  const featured = [...accounts].sort((left, right) => left.trend_pct - right.trend_pct)[0];
  const recentValues = featured.weekly_logins.slice(-4);
  const previousValues = featured.weekly_logins.slice(-8, -4);
  const bars = recentValues.map((value, index) => {
    const baseline = previousValues[index] ?? featured.previous_avg;
    const width = Math.max(8, Math.min(100, (value / Math.max(baseline, 1)) * 100));
    return `
      <div class="stage-bar-row">
        <span>Week ${featured.history_weeks - 3 + index}</span>
        <div class="stage-bar-track"><div class="stage-bar-fill" style="width: ${width}%"></div></div>
        <strong>${value}</strong>
      </div>
    `;
  }).join('');

  document.getElementById('featured-summary').textContent = `${featured.account_name} is the clearest intervention candidate: ${formatTrend(featured.trend_pct)} versus its prior baseline, ${featured.below_baseline_weeks} of the last 4 weeks below baseline, and enough history to treat the signal as credible.`;

  document.getElementById('story-points').innerHTML = [
    {
      title: `${featured.account_id} on ${featured.plan_tier}`,
      body: 'The account sits in a valuable tier, so a sustained usage drop matters commercially as well as behaviorally.'
    },
    {
      title: `${featured.recent_avg.toFixed(1)} recent average`,
      body: `That is down from ${featured.previous_avg.toFixed(1)} in the prior comparison window.`
    },
    {
      title: `${featured.below_baseline_weeks} weak recent weeks`,
      body: 'The decline is persistent rather than a one-week anomaly, which is why it is escalated.'
    }
  ].map((point) => `
    <article class="story-point reveal">
      <strong>${point.title}</strong>
      <p>${point.body}</p>
    </article>
  `).join('');

  document.getElementById('featured-stage').innerHTML = `
    <section class="featured-stage reveal">
      <div class="stage-topline">
        <div>
          <span class="stage-kicker">Featured risk narrative</span>
          <h3 class="stage-title">${featured.account_name}</h3>
        </div>
        <span class="badge ${badgeClass(featured.risk_label)}">${featured.risk_label.replace(/_/g, ' ')}</span>
      </div>
      <p class="stage-summary">${featured.reason}</p>
      <div class="stage-grid">
        <div class="stage-chart">
          <article class="stage-chart-card">
            <p>Recent weekly logins compared with the account's earlier baseline.</p>
            <div class="stage-bars">${bars}</div>
          </article>
          <article class="stage-chart-card">
            ${buildSparkline(featured.weekly_logins)}
          </article>
        </div>
        <div class="stage-side">
          <article class="stage-stat">
            <strong>${formatTrend(featured.trend_pct)}</strong>
            <span>Trend versus prior 4-week baseline</span>
          </article>
          <article class="stage-stat">
            <strong>${featured.history_weeks}</strong>
            <span>Total weeks of observed history</span>
          </article>
          <article class="stage-side-card">
            <p>The rule set only escalates mature accounts with material and sustained decline, which keeps this recommendation explainable and defensible.</p>
          </article>
        </div>
      </div>
    </section>
  `;
}

function renderPortfolioGallery(accounts) {
  const ordered = [...accounts].sort((left, right) => left.trend_pct - right.trend_pct);
  document.getElementById('portfolio-gallery').innerHTML = ordered.map((account) => `
    <article class="gallery-card reveal">
      <span class="badge ${badgeClass(account.risk_label)}">${account.risk_label.replace(/_/g, ' ')}</span>
      <strong>${account.account_name}</strong>
      <p>${account.plan_tier} plan · ${account.history_weeks} weeks · <span class="${trendClass(account.trend_pct)}">${formatTrend(account.trend_pct)}</span></p>
      <p>${account.reason}</p>
    </article>
  `).join('');
}

function setupRevealAnimations() {
  const elements = document.querySelectorAll('.reveal');
  if (!('IntersectionObserver' in window)) {
    elements.forEach((element) => element.classList.add('is-visible'));
    return;
  }

  const observer = new IntersectionObserver((entries) => {
    entries.forEach((entry) => {
      if (entry.isIntersecting) {
        entry.target.classList.add('is-visible');
        observer.unobserve(entry.target);
      }
    });
  }, { threshold: 0.18 });

  elements.forEach((element) => observer.observe(element));
}

function renderFlagged(accounts) {
  const priority = { HIGH_RISK: 0, MEDIUM_RISK: 1, MONITOR: 2, INSUFFICIENT_DATA: 3, HEALTHY: 4 };
  const ordered = [...accounts].sort((left, right) => {
    const riskDelta = priority[left.risk_label] - priority[right.risk_label];
    if (riskDelta !== 0) return riskDelta;
    return left.trend_pct - right.trend_pct;
  });

  document.getElementById('flagged-list').innerHTML = ordered.map((account) => `
    <article class="flag-card">
      <div class="flag-card-header">
        <div>
          <p class="section-label">${account.account_id} · ${account.plan_tier}</p>
          <h3>${account.account_name}</h3>
        </div>
        <span class="badge ${badgeClass(account.risk_label)}">${account.risk_label.replace(/_/g, ' ')}</span>
      </div>
      <p>${account.reason}</p>
      <div class="flag-metrics">
        <div class="metric-tile"><strong>${formatTrend(account.trend_pct)}</strong><span>Recent vs prior trend</span></div>
        <div class="metric-tile"><strong>${account.recent_avg.toFixed(1)}</strong><span>Recent 4-week average</span></div>
        <div class="metric-tile"><strong>${account.previous_avg.toFixed(1)}</strong><span>Prior 4-week average</span></div>
        <div class="metric-tile"><strong>${account.history_weeks}</strong><span>Weeks of history</span></div>
        <div class="metric-tile"><strong>${account.below_baseline_weeks}</strong><span>Recent weeks below baseline</span></div>
        <div class="sparkline-box">${buildSparkline(account.weekly_logins)}</div>
      </div>
    </article>
  `).join('');
}

function renderTable(accounts) {
  document.getElementById('account-table-body').innerHTML = accounts.map((account) => `
    <tr>
      <td><strong>${account.account_name}</strong><br>${account.account_id}</td>
      <td>${account.plan_tier}</td>
      <td>${account.history_weeks} weeks</td>
      <td>${account.recent_avg.toFixed(1)}</td>
      <td>${account.previous_avg.toFixed(1)}</td>
      <td class="${trendClass(account.trend_pct)}">${formatTrend(account.trend_pct)}</td>
      <td><span class="badge ${badgeClass(account.risk_label)}">${account.risk_label.replace(/_/g, ' ')}</span></td>
      <td>${account.reason}</td>
    </tr>
  `).join('');
}

loadAnalysis()
  .then((data) => {
    renderSummary(data.accounts);
    renderHeroOrbit(data.accounts);
    renderFeaturedRisk(data.accounts);
    renderDatasetStory(data.accounts);
    renderDecisionSteps();
    renderFlagged(data.accounts);
    renderPortfolioStrip(data.accounts);
    renderPortfolioGallery(data.accounts);
    renderTable(data.accounts);
    setupRevealAnimations();
  })
  .catch((error) => {
    document.body.innerHTML = `<main class="page-shell"><section class="panel"><h1>Unable to load analysis</h1><p>${error.message}</p></section></main>`;
  });
