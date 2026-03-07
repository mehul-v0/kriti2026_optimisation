import { useState, useMemo } from 'react';
import { Link } from 'react-router-dom';
import { motion, AnimatePresence } from 'framer-motion';
import { useApp } from '../context/AppContext';
import type { ConstraintDetail } from '../types';

/* ── icon helper ── */
const Icon = ({ name, className = '' }: { name: string; className?: string }) => (
  <span className={`material-symbols-outlined ${className}`}>{name}</span>
);

/* ── Compliance Donut (SVG) ── */
function ComplianceDonut({ rate, satisfied, total }: { rate: number; satisfied: number; total: number }) {
  const r = 54;
  const circ = 2 * Math.PI * r;
  const filled = (rate / 100) * circ;
  return (
    <div className="relative flex items-center justify-center">
      <svg width="160" height="160" viewBox="0 0 128 128">
        <circle cx="64" cy="64" r={r} fill="none" stroke="rgba(255,255,255,0.06)" strokeWidth="10" />
        <circle
          cx="64" cy="64" r={r} fill="none"
          stroke="#FFB800" strokeWidth="10" strokeLinecap="round"
          strokeDasharray={`${filled} ${circ - filled}`}
          strokeDashoffset={circ * 0.25}
          className="transition-all duration-700"
        />
      </svg>
      <div className="absolute inset-0 flex flex-col items-center justify-center">
        <span className="text-3xl font-extrabold text-white">{Math.round(rate)}%</span>
        <span className="text-[10px] font-mono text-white/30 mt-0.5">{satisfied} of {total}</span>
      </div>
    </div>
  );
}

/* ── Progress Bar ── */
function ProgressBar({ label, value, color }: { label: string; value: number; color: string }) {
  return (
    <div>
      <div className="flex items-center justify-between mb-1.5">
        <span className="text-sm font-medium text-white/70">{label}</span>
        <span className="text-sm font-bold" style={{ color }}>{Math.round(value)}%</span>
      </div>
      <div className="h-2 rounded-full bg-white/[0.06] overflow-hidden">
        <motion.div
          initial={{ width: 0 }}
          animate={{ width: `${value}%` }}
          transition={{ duration: 0.8, ease: 'easeOut' }}
          className="h-full rounded-full"
          style={{ background: `linear-gradient(90deg, ${color}, ${color}cc)` }}
        />
      </div>
    </div>
  );
}

/* ── Constraint Row ── */
function ConstraintRow({ constraint, index }: { constraint: ConstraintDetail; index: number }) {
  const [expanded, setExpanded] = useState(false);
  const hasViolations = constraint.violations && constraint.violations.length > 0;
  const isViolated = constraint.status !== 'satisfied';
  const canExpand = hasViolations || isViolated;

  return (
    <motion.div
      initial={{ opacity: 0, y: 12 }}
      animate={{ opacity: 1, y: 0 }}
      transition={{ delay: index * 0.04 }}
      className="bg-panel-dark border border-white/10 overflow-visible"
    >
      <button
        onClick={() => canExpand && setExpanded(!expanded)}
        className={`w-full px-5 py-3.5 flex items-center justify-between text-left ${canExpand ? 'cursor-pointer hover:bg-white/[0.02] transition-colors' : 'cursor-default'}`}
      >
        <div className="flex items-center gap-3 min-w-0">
          {constraint.status === 'satisfied' ? (
            <Icon name="check_circle" className="text-primary text-xl flex-shrink-0" />
          ) : constraint.status === 'violated' ? (
            <Icon name="error" className="text-red-400 text-xl flex-shrink-0" />
          ) : (
            <Icon name="warning" className="text-yellow-400 text-xl flex-shrink-0" />
          )}
          <div className="flex items-center gap-2 min-w-0">
            <h3 className="font-semibold text-sm text-white/90 truncate">{constraint.name}</h3>
            <div className="group relative flex-shrink-0">
              <Icon name="info" className="text-base text-white/30 hover:text-primary transition-colors cursor-help" />
              <div className="absolute left-1/2 -translate-x-1/2 bottom-full mb-2 w-64 p-3 rounded-lg text-xs text-white/80 opacity-0 invisible group-hover:opacity-100 group-hover:visible transition-all duration-200 z-[100] pointer-events-none bg-panel-dark border border-white/10 shadow-xl">
                {constraint.description}
                <div className="absolute left-1/2 -translate-x-1/2 top-full w-0 h-0 border-l-4 border-r-4 border-t-4 border-transparent border-t-primary/20" />
              </div>
            </div>
          </div>
        </div>
        <div className="flex items-center gap-3 flex-shrink-0">
          <span className={`text-xs font-semibold px-3 py-1 ${
            constraint.status === 'satisfied'
              ? 'bg-primary/10 text-primary border border-primary/20'
              : constraint.status === 'violated'
              ? 'bg-red-500/10 text-red-400 border border-red-500/20'
              : 'bg-yellow-500/10 text-yellow-400 border border-yellow-500/20'
          }`}>
            {constraint.status === 'satisfied' ? 'Pass' : constraint.status === 'violated' ? 'Fail' : 'Relaxed'}
          </span>
          {canExpand && (
            <Icon name="expand_more" className={`text-lg text-white/30 transition-transform duration-200 ${expanded ? 'rotate-180' : ''}`} />
          )}
        </div>
      </button>

      <AnimatePresence>
        {expanded && (
          <motion.div
            initial={{ height: 0, opacity: 0 }}
            animate={{ height: 'auto', opacity: 1 }}
            exit={{ height: 0, opacity: 0 }}
            transition={{ duration: 0.2 }}
            className="overflow-hidden"
          >
            <div className="px-5 pb-4 pt-1 border-t border-white/[0.05]">
              {hasViolations ? (
                <div className="space-y-2 mt-3">
                  <p className="text-[11px] font-semibold uppercase tracking-widest text-white/30 mb-2">
                    Violations ({constraint.violations.length})
                  </p>
                  {constraint.violations.map((v: any, i: number) => (
                    <div key={i} className="bg-white/[0.03] px-3 py-2.5 text-sm text-white/70 border border-white/[0.04]">
                      {typeof v === 'string' ? v : (
                        <div className="flex flex-wrap gap-x-4 gap-y-1 text-xs">
                          {Object.entries(v).map(([key, val]) => (
                            <span key={key}>
                              <span className="text-white/35">{key}:</span>{' '}
                              <span className="text-white/75">{String(val)}</span>
                            </span>
                          ))}
                        </div>
                      )}
                    </div>
                  ))}
                </div>
              ) : (
                <p className="text-xs text-white/40 mt-3">
                  This constraint was {constraint.status} but no specific violation details are available.
                </p>
              )}
            </div>
          </motion.div>
        )}
      </AnimatePresence>
    </motion.div>
  );
}

/* ── severity helper ── */
function getSeverity(constraint: ConstraintDetail, isHard: boolean): 'Critical' | 'Warning' | 'Info' {
  if (isHard && constraint.status === 'violated') return 'Critical';
  if (constraint.status === 'violated') return 'Warning';
  if (constraint.status === 'relaxed') return 'Info';
  return 'Info';
}

/* ══════════════════════ MAIN COMPONENT ══════════════════════ */
export default function ConstraintValidation() {
  const { currentResult } = useApp();

  if (!currentResult) {
    return (
      <div className="min-h-screen flex items-center justify-center">
        <div className="text-center">
          <Icon name="verified" className="text-5xl text-primary/30 mb-4" />
          <p className="text-white/50 mb-4">No optimization results available</p>
          <Link to="/upload" className="btn-primary inline-flex items-center gap-2">
            <Icon name="upload" className="text-lg" /> Start New Optimization
          </Link>
        </div>
      </div>
    );
  }

  const { hard, soft } = currentResult.constraints;
  const totalConstraints = hard.total + soft.total;
  const totalSatisfied = hard.satisfied + soft.satisfied;
  const overallRate = totalConstraints > 0 ? (totalSatisfied / totalConstraints) * 100 : 0;

  /* collect all violated / relaxed for the summary table */
  const violationRows = useMemo(() => {
    const rows: { name: string; severity: 'Critical' | 'Warning' | 'Info'; affected: number; type: 'Hard' | 'Soft'; status: string }[] = [];
    hard.details.filter(c => c.status !== 'satisfied').forEach(c => {
      rows.push({ name: c.name, severity: getSeverity(c, true), affected: c.violations?.length ?? 0, type: 'Hard', status: c.status });
    });
    soft.details.filter(c => c.status !== 'satisfied').forEach(c => {
      rows.push({ name: c.name, severity: getSeverity(c, false), affected: c.violations?.length ?? 0, type: 'Soft', status: c.status });
    });
    return rows;
  }, [hard, soft]);

  return (
    <div className="min-h-screen p-6 lg:p-8">
      <div className="max-w-[1400px] mx-auto">

        {/* ── Page Header ── */}
        <motion.div initial={{ opacity: 0, y: -12 }} animate={{ opacity: 1, y: 0 }} className="mb-8">
          <div className="flex items-center gap-3 mb-1">
            <Icon name="verified" className="text-primary text-3xl" />
            <h1 className="text-2xl font-black text-white tracking-tight uppercase">Constraint Compliance Report</h1>
          </div>
          <p className="text-xs font-mono text-white/30 ml-11">
            {totalSatisfied} of {totalConstraints} constraints fully satisfied &middot; Generated {new Date(currentResult.timestamp).toLocaleString()}
          </p>
        </motion.div>

        {/* ══ MAIN 12-COL GRID (content 9 + sidebar 3) ══ */}
        <div className="grid grid-cols-1 xl:grid-cols-12 gap-6">

          {/* ── LEFT CONTENT (9 cols) ── */}
          <div className="xl:col-span-9 space-y-6">

            {/* ── TOP: Compliance Overview (5+7 grid) ── */}
            <motion.div initial={{ opacity: 0, y: 20 }} animate={{ opacity: 1, y: 0 }} className="grid grid-cols-1 md:grid-cols-12 gap-6">

              {/* Overall Compliance (5-col) */}
              <div className="md:col-span-5 bg-panel-dark border border-white/10 p-6 flex flex-col items-center justify-center">
                <h2 className="text-[11px] font-label font-bold uppercase tracking-widest text-white/40 mb-4">Overall Compliance</h2>
                <ComplianceDonut rate={overallRate} satisfied={totalSatisfied} total={totalConstraints} />
                <p className="text-[11px] text-white/30 mt-3 text-center">
                  {hard.violated + soft.violated === 0
                    ? 'All constraints satisfied'
                    : `${hard.violated + soft.violated} constraint${hard.violated + soft.violated > 1 ? 's' : ''} need attention`}
                </p>
              </div>

              {/* Hard + Soft Progress (7-col) */}
              <div className="md:col-span-7 bg-panel-dark border border-white/10 p-6 flex flex-col justify-center space-y-6">
                {/* Hard */}
                <div>
                  <div className="flex items-center gap-2 mb-3">
                    <Icon name="shield" className="text-primary text-lg" />
                    <span className="text-xs font-label font-bold uppercase tracking-wider text-white/50">Hard Constraints</span>
                    <span className="ml-auto text-xs text-white/35">{hard.satisfied}/{hard.total} passed</span>
                  </div>
                  <ProgressBar label="" value={hard.complianceRate} color="#FFB800" />
                  <div className="flex gap-4 mt-2 text-[11px] text-white/35">
                    <span>Satisfied: <span className="text-primary font-semibold">{hard.satisfied}</span></span>
                    <span>Violated: <span className="text-red-400 font-semibold">{hard.violated}</span></span>
                  </div>
                </div>

                {/* Soft */}
                <div>
                  <div className="flex items-center gap-2 mb-3">
                    <Icon name="tune" className="text-yellow-400 text-lg" />
                    <span className="text-xs font-label font-bold uppercase tracking-wider text-white/50">Soft Constraints</span>
                    <span className="ml-auto text-xs text-white/35">{soft.satisfied}/{soft.total} passed</span>
                  </div>
                  <ProgressBar label="" value={soft.complianceRate} color="#facc15" />
                  <div className="flex gap-4 mt-2 text-[11px] text-white/35">
                    <span>Satisfied: <span className="text-yellow-400 font-semibold">{soft.satisfied}</span></span>
                    <span>Relaxed / Violated: <span className="text-yellow-400/70 font-semibold">{soft.violated}</span></span>
                  </div>
                </div>
              </div>
            </motion.div>

            {/* ── Hard Constraints List ── */}
            <motion.div initial={{ opacity: 0, y: 20 }} animate={{ opacity: 1, y: 0 }} transition={{ delay: 0.1 }}>
              <div className="flex items-center justify-between mb-4">
                <div className="flex items-center gap-2">
                  <Icon name="shield" className="text-primary text-xl" />
                  <h2 className="text-[11px] font-label font-bold uppercase tracking-widest text-white/40">Hard Constraints</h2>
                </div>
                <span className="text-[11px] text-white/30 uppercase tracking-wider">Must never be violated</span>
              </div>
              <div className="space-y-2.5">
                {hard.details.length > 0 ? hard.details.map((c, i) => (
                  <ConstraintRow key={i} constraint={c} index={i} />
                )) : (
                  <p className="text-white/35 text-sm">No hard constraint details available</p>
                )}
              </div>
            </motion.div>

            {/* ── Soft Constraints List ── */}
            <motion.div initial={{ opacity: 0, y: 20 }} animate={{ opacity: 1, y: 0 }} transition={{ delay: 0.2 }}>
              <div className="flex items-center justify-between mb-4">
                <div className="flex items-center gap-2">
                  <Icon name="tune" className="text-yellow-400 text-xl" />
                  <h2 className="text-[11px] font-label font-bold uppercase tracking-widest text-white/40">Soft Constraints</h2>
                </div>
                <span className="text-[11px] text-white/30 uppercase tracking-wider">Can be relaxed for optimization</span>
              </div>
              <div className="space-y-2.5">
                {soft.details.length > 0 ? soft.details.map((c, i) => (
                  <ConstraintRow key={i} constraint={c} index={i} />
                )) : (
                  <p className="text-white/35 text-sm">No soft constraint details available</p>
                )}
              </div>
            </motion.div>

            {/* ── Violation Summary Table ── */}
            {violationRows.length > 0 && (
              <motion.div initial={{ opacity: 0, y: 20 }} animate={{ opacity: 1, y: 0 }} transition={{ delay: 0.3 }}>
                <div className="flex items-center gap-2 mb-4">
                  <Icon name="table_chart" className="text-red-400 text-xl" />
                  <h2 className="text-[11px] font-label font-bold uppercase tracking-widest text-white/40">Violation Summary</h2>
                </div>
                <div className="bg-panel-dark border border-white/10 overflow-hidden">
                  <table className="w-full text-sm">
                    <thead>
                      <tr className="border-b border-white/[0.06]">
                        <th className="text-left px-5 py-3 text-[11px] uppercase tracking-widest text-white/30 font-semibold">Constraint Breach</th>
                        <th className="text-left px-5 py-3 text-[11px] uppercase tracking-widest text-white/30 font-semibold">Type</th>
                        <th className="text-left px-5 py-3 text-[11px] uppercase tracking-widest text-white/30 font-semibold">Severity</th>
                        <th className="text-left px-5 py-3 text-[11px] uppercase tracking-widest text-white/30 font-semibold">Violations</th>
                        <th className="text-left px-5 py-3 text-[11px] uppercase tracking-widest text-white/30 font-semibold">Status</th>
                      </tr>
                    </thead>
                    <tbody>
                      {violationRows.map((row, i) => (
                        <tr key={i} className="border-b border-white/[0.03] hover:bg-white/[0.02] transition-colors">
                          <td className="px-5 py-3 text-white/80 font-medium">{row.name}</td>
                          <td className="px-5 py-3">
                            <span className={`text-xs font-semibold px-2 py-0.5 ${row.type === 'Hard' ? 'bg-primary/10 text-primary' : 'bg-yellow-500/10 text-yellow-400'}`}>
                              {row.type}
                            </span>
                          </td>
                          <td className="px-5 py-3">
                            <span className={`inline-flex items-center gap-1 text-xs font-semibold ${
                              row.severity === 'Critical' ? 'text-red-400' : row.severity === 'Warning' ? 'text-yellow-400' : 'text-white/40'
                            }`}>
                              <Icon name={row.severity === 'Critical' ? 'error' : row.severity === 'Warning' ? 'warning' : 'info'} className="text-sm" />
                              {row.severity}
                            </span>
                          </td>
                          <td className="px-5 py-3 text-white/60">{row.affected}</td>
                          <td className="px-5 py-3">
                            <span className={`text-xs font-semibold px-2.5 py-1 ${
                              row.status === 'violated' ? 'bg-red-500/10 text-red-400 border border-red-500/20' : 'bg-yellow-500/10 text-yellow-400 border border-yellow-500/20'
                            }`}>
                              {row.status === 'violated' ? 'Fail' : 'Relaxed'}
                            </span>
                          </td>
                        </tr>
                      ))}
                    </tbody>
                  </table>
                </div>
              </motion.div>
            )}
          </div>

          {/* ── RIGHT SIDEBAR (3 cols) ── */}
          <aside className="xl:col-span-3 space-y-6 xl:sticky xl:bottom-6 xl:self-end">

            {/* Quick Stats */}
            <motion.div initial={{ opacity: 0, x: 20 }} animate={{ opacity: 1, x: 0 }} className="bg-panel-dark border border-white/10 p-5">
              <h3 className="text-[11px] font-label font-bold uppercase tracking-widest text-white/40 mb-4">Quick Stats</h3>
              <div className="space-y-4">
                <div className="flex items-center gap-3">
                  <div className="w-9 h-9 bg-primary/10 flex items-center justify-center">
                    <Icon name="check_circle" className="text-primary text-lg" />
                  </div>
                  <div>
                    <p className="text-lg font-bold text-white">{totalSatisfied}</p>
                    <p className="text-[11px] text-white/35">Satisfied</p>
                  </div>
                </div>
                <div className="flex items-center gap-3">
                  <div className="w-9 h-9 bg-red-500/10 flex items-center justify-center">
                    <Icon name="cancel" className="text-red-400 text-lg" />
                  </div>
                  <div>
                    <p className="text-lg font-bold text-white">{hard.violated + soft.violated}</p>
                    <p className="text-[11px] text-white/35">Violated / Relaxed</p>
                  </div>
                </div>
                <div className="flex items-center gap-3">
                  <div className="w-9 h-9 bg-white/[0.05] flex items-center justify-center">
                    <Icon name="functions" className="text-white/50 text-lg" />
                  </div>
                  <div>
                    <p className="text-lg font-bold text-white">{totalConstraints}</p>
                    <p className="text-[11px] text-white/35">Total Constraints</p>
                  </div>
                </div>
              </div>
            </motion.div>

            {/* Compliance Breakdown Visualization */}
            <motion.div initial={{ opacity: 0, x: 20 }} animate={{ opacity: 1, x: 0 }} transition={{ delay: 0.1 }} className="bg-panel-dark border border-white/10 p-5">
              <h3 className="text-[11px] font-label font-bold uppercase tracking-widest text-white/40 mb-4">Compliance Breakdown</h3>
              {/* Stacked horizontal bar */}
              <div className="space-y-3">
                <div>
                  <div className="flex justify-between text-[11px] mb-1">
                    <span className="text-white/50">Hard</span>
                    <span className="text-primary font-semibold">{hard.complianceRate}%</span>
                  </div>
                  <div className="flex h-3 rounded-full overflow-hidden bg-white/[0.04]">
                    <div className="bg-primary rounded-l-full transition-all duration-700" style={{ width: `${hard.total > 0 ? (hard.satisfied / hard.total) * 100 : 0}%` }} />
                    {hard.violated > 0 && (
                      <div className="bg-red-500/70 transition-all duration-700" style={{ width: `${(hard.violated / hard.total) * 100}%` }} />
                    )}
                  </div>
                </div>
                <div>
                  <div className="flex justify-between text-[11px] mb-1">
                    <span className="text-white/50">Soft</span>
                    <span className="text-yellow-400 font-semibold">{soft.complianceRate}%</span>
                  </div>
                  <div className="flex h-3 rounded-full overflow-hidden bg-white/[0.04]">
                    <div className="bg-yellow-400 rounded-l-full transition-all duration-700" style={{ width: `${soft.total > 0 ? (soft.satisfied / soft.total) * 100 : 0}%` }} />
                    {soft.violated > 0 && (
                      <div className="bg-red-500/70 transition-all duration-700" style={{ width: `${(soft.violated / soft.total) * 100}%` }} />
                    )}
                  </div>
                </div>
              </div>

              {/* Legend */}
              <div className="flex flex-wrap gap-3 mt-4 text-[11px]">
                <span className="flex items-center gap-1.5"><span className="w-2 h-2 rounded-full bg-primary" />Satisfied</span>
                <span className="flex items-center gap-1.5"><span className="w-2 h-2 rounded-full bg-red-500/70" />Violated</span>
                <span className="flex items-center gap-1.5"><span className="w-2 h-2 rounded-full bg-yellow-400" />Soft</span>
              </div>
            </motion.div>

            {/* Insight Card */}
            <motion.div initial={{ opacity: 0, x: 20 }} animate={{ opacity: 1, x: 0 }} transition={{ delay: 0.2 }} className="bg-panel-dark border border-white/10 p-5">
              <h3 className="text-[11px] font-label font-bold uppercase tracking-widest text-white/40 mb-3">Insight</h3>
              <div className="flex items-start gap-3">
                <Icon name="lightbulb" className="text-primary text-xl mt-0.5 flex-shrink-0" />
                <p className="text-xs text-white/50 leading-relaxed">
                  {hard.violated === 0 && soft.violated === 0
                    ? 'All constraints are fully satisfied. The optimization solution is valid and compliant with every rule.'
                    : hard.violated > 0
                    ? `${hard.violated} hard constraint${hard.violated > 1 ? 's are' : ' is'} violated — the solution may be invalid. Review the Hard Constraints section for critical issues.`
                    : `All hard constraints pass. ${soft.violated} soft constraint${soft.violated > 1 ? 's were' : ' was'} relaxed for a better-optimized result.`}
                </p>
              </div>
            </motion.div>

            {/* Solver Info */}
            <motion.div initial={{ opacity: 0, x: 20 }} animate={{ opacity: 1, x: 0 }} transition={{ delay: 0.3 }} className="bg-panel-dark border border-white/10 p-5">
              <h3 className="text-[11px] font-label font-bold uppercase tracking-widest text-white/40 mb-3">Solver Info</h3>
              <div className="space-y-2 text-xs">
                <div className="flex justify-between">
                  <span className="text-white/35">Mode</span>
                  <span className="text-white/70 font-medium">{currentResult.solverMode}</span>
                </div>
                <div className="flex justify-between">
                  <span className="text-white/35">Duration</span>
                  <span className="text-white/70 font-medium">{currentResult.solverDuration?.toFixed(1) ?? '—'}s</span>
                </div>
                <div className="flex justify-between">
                  <span className="text-white/35">Trips</span>
                  <span className="text-white/70 font-medium">{currentResult.trips.length}</span>
                </div>
              </div>
            </motion.div>
          </aside>

        </div>
      </div>
    </div>
  );
}

