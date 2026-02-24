import { useState } from 'react';
import { Link } from 'react-router-dom';
import { motion, AnimatePresence } from 'framer-motion';
import { useApp } from '../context/AppContext';
import { Shield, CheckCircle, AlertTriangle, ChevronDown, Info } from 'lucide-react';
import type { ConstraintDetail } from '../types';

const cardClass = "bg-dark-800/60 backdrop-blur-xl rounded-2xl border border-gray/10 shadow-float hover:shadow-float-lg transition-all duration-300 ";

function ConstraintRow({ constraint }: { constraint: ConstraintDetail }) {
  const [expanded, setExpanded] = useState(false);
  const hasViolations = constraint.violations && constraint.violations.length > 0;
  const isViolated = constraint.status !== 'satisfied';

  return (
    <div className={`${cardClass} overflow-visible`}>
      <button
        onClick={() => (hasViolations || isViolated) && setExpanded(!expanded)}
        className={`w-full p-4 flex items-center justify-between text-left ${(hasViolations || isViolated) ? 'cursor-pointer hover:bg-white/[0.02] transition-colors' : 'cursor-default'}`}
      >
        <div className="flex items-center gap-3">
          {constraint.status === 'satisfied' ? (
            <CheckCircle className="w-5 h-5 text-primary flex-shrink-0" />
          ) : (
            <AlertTriangle className="w-5 h-5 text-red-400 flex-shrink-0" />
          )}
          <div className="flex items-center gap-2">
            <h3 className="font-bold text-sm">{constraint.name}</h3>
            <div className="group relative">
              <Info className="w-4 h-4 text-gray/40 hover:text-primary transition-colors cursor-help" />
              <div className="absolute left-1/2 -translate-x-1/2 bottom-full mb-2 w-64 p-3 bg-dark-800 border border-gray/20 rounded-lg text-xs text-gray/90 opacity-0 invisible group-hover:opacity-100 group-hover:visible transition-all duration-200 z-[100] shadow-xl pointer-events-none">
                {constraint.description}
                <div className="absolute left-1/2 -translate-x-1/2 top-full w-0 h-0 border-l-4 border-r-4 border-t-4 border-transparent border-t-gray/20" />
              </div>
            </div>
          </div>
        </div>
        <div className="flex items-center gap-3 flex-shrink-0">
          <span className={`text-xs font-medium px-3 py-1 rounded-full ${
            constraint.status === 'satisfied'
              ? 'bg-primary/10 text-primary'
              : constraint.status === 'violated'
              ? 'bg-red-500/10 text-red-400'
              : 'bg-yellow-500/10 text-yellow-400'
          }`}>
            {constraint.status === 'satisfied' ? '✓ Satisfied' : constraint.status === 'violated' ? '⚠ Violated' : '~ Relaxed'}
          </span>
          {(hasViolations || isViolated) && (
            <ChevronDown className={`w-4 h-4 text-gray/40 transition-transform duration-200 ${expanded ? 'rotate-180' : ''}`} />
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
            <div className="px-4 pb-4 pt-1 border-t border-gray/10">
              {hasViolations ? (
                <div className="space-y-2 mt-3">
                  <p className="text-xs font-semibold uppercase tracking-wider text-gray/40 mb-2">
                    Violations ({constraint.violations.length})
                  </p>
                  {constraint.violations.map((v: any, i: number) => (
                    <div key={i} className="bg-dark-700/50 rounded-lg px-3 py-2 text-sm text-gray/80">
                      {typeof v === 'string' ? v : (
                        <div className="flex flex-wrap gap-x-4 gap-y-1 text-xs">
                          {Object.entries(v).map(([key, val]) => (
                            <span key={key}>
                              <span className="text-gray/40">{key}:</span>{' '}
                              <span className="text-white/80">{String(val)}</span>
                            </span>
                          ))}
                        </div>
                      )}
                    </div>
                  ))}
                </div>
              ) : (
                <p className="text-xs text-gray/50 mt-3">
                  This constraint was {constraint.status} but no specific violation details are available.
                </p>
              )}
            </div>
          </motion.div>
        )}
      </AnimatePresence>
    </div>
  );
}

export default function ConstraintValidation() {
  const { currentResult } = useApp();

  if (!currentResult) {
    return (
      <div className="min-h-screen bg-dark flex items-center justify-center">
        <div className="text-center">
          <p className="text-gray mb-4">No optimization results available</p>
          <Link to="/upload" className="btn-primary">Start New Optimization</Link>
        </div>
      </div>
    );
  }

  const { hard, soft } = currentResult.constraints;
  const totalConstraints = hard.total + soft.total;
  const totalSatisfied = hard.satisfied + soft.satisfied;

  return (
    <div className="min-h-screen bg-dark p-8">
      <div className="max-w-6xl mx-auto">
        <div className="mb-8">
          <h1 className="text-4xl font-bold mb-2">Constraint Compliance Report</h1>
          <div className="flex items-center gap-3">
            <span className="text-sm text-gray/60">
              {totalSatisfied} of {totalConstraints} constraints fully satisfied
            </span>
          </div>
        </div>

        {/* Compliance Overview */}
        <motion.div initial={{ opacity: 0, y: 20 }} animate={{ opacity: 1, y: 0 }} className="mb-8">
          <h2 className="text-2xl font-bold mb-4">Compliance Overview</h2>
          <div className="grid grid-cols-1 md:grid-cols-2 gap-6">
            {/* Hard Constraints Card */}
            <div className={`${cardClass} p-6`}>
              <div className="flex items-center gap-3 mb-5">
                <Shield className="w-7 h-7 text-primary" />
                <h3 className="text-lg font-bold">Hard Constraints</h3>
              </div>
              <div className="space-y-3">
                <div className="flex justify-between items-center">
                  <span className="text-sm font-medium text-white/70">Total</span>
                  <span className="text-lg font-bold text-white">{hard.total}</span>
                </div>
                <div className="flex justify-between items-center">
                  <span className="text-sm font-medium text-white/70">Satisfied</span>
                  <span className="text-lg font-bold text-primary">{hard.satisfied}</span>
                </div>
                <div className="flex justify-between items-center">
                  <span className="text-sm font-medium text-white/70">Violated</span>
                  <span className="text-lg font-bold text-primary">{hard.violated}</span>
                </div>
                <div className="flex justify-between items-center">
                  <span className="text-sm font-medium text-white/70">Compliance Rate</span>
                  <div className="flex items-center gap-3">
                    <span className="text-lg font-bold text-primary">{hard.complianceRate}%</span>
                    <div className="flex gap-1">
                      {Array.from({ length: 10 }, (_, i) => (
                        <div
                          key={i}
                          className={`w-1 h-4 rounded ${(i + 1) <= (hard.complianceRate / 10) ? 'bg-primary' : 'bg-dark-500'}`}
                        />
                      ))}
                    </div>
                  </div>
                </div>
              </div>
            </div>

            {/* Soft Constraints Card */}
            <div className={`${cardClass} p-6`}>
              <div className="flex items-center gap-3 mb-5">
                <Shield className="w-7 h-7 text-primary" />
                <h3 className="text-lg font-bold">Soft Constraints</h3>
              </div>
              <div className="space-y-3">
                <div className="flex justify-between items-center">
                  <span className="text-sm font-medium text-white/70">Total</span>
                  <span className="text-lg font-bold text-white">{soft.total}</span>
                </div>
                <div className="flex justify-between items-center">
                  <span className="text-sm font-medium text-white/70">Satisfied</span>
                  <span className="text-lg font-bold text-primary">{soft.satisfied}</span>
                </div>
                <div className="flex justify-between items-center">
                  <span className="text-sm font-medium text-white/70">Relaxed/Violated</span>
                  <span className="text-lg font-bold text-primary">{soft.violated}</span>
                </div>
                <div className="flex justify-between items-center">
                  <span className="text-sm font-medium text-white/70">Compliance Rate</span>
                  <div className="flex items-center gap-3">
                    <span className="text-lg font-bold text-primary">{soft.complianceRate}%</span>
                    <div className="flex gap-1">
                      {Array.from({ length: 10 }, (_, i) => (
                        <div
                          key={i}
                          className={`w-1 h-4 rounded ${(i + 1) <= (soft.complianceRate / 10) ? 'bg-primary' : 'bg-dark-500'}`}
                        />
                      ))}
                    </div>
                  </div>
                </div>
              </div>
            </div>
          </div>
        </motion.div>

        {/* Hard Constraints Detail */}
        <motion.div initial={{ opacity: 0, y: 20 }} animate={{ opacity: 1, y: 0 }} transition={{ delay: 0.1 }} className="mb-8">
          <div className="flex items-center justify-between mb-4">
            <h2 className="text-2xl font-bold">Hard Constraints</h2>
            <span className="text-xs text-gray/40">Must never be violated for a valid solution</span>
          </div>
          <div className="space-y-3">
            {hard.details.length > 0 ? hard.details.map((constraint, index) => (
              <ConstraintRow key={index} constraint={constraint} />
            )) : (
              <p className="text-gray/50 text-sm">No hard constraint details available</p>
            )}
          </div>
        </motion.div>

        {/* Soft Constraints Detail */}
        <motion.div initial={{ opacity: 0, y: 20 }} animate={{ opacity: 1, y: 0 }} transition={{ delay: 0.2 }}>
          <div className="flex items-center justify-between mb-4">
            <h2 className="text-2xl font-bold">Soft Constraints</h2>
            <span className="text-xs text-gray/40">Preferences that may be relaxed for better optimization</span>
          </div>
          <div className="space-y-3">
            {soft.details.length > 0 ? soft.details.map((constraint, index) => (
              <ConstraintRow key={index} constraint={constraint} />
            )) : (
              <p className="text-gray/50 text-sm">No soft constraint details available</p>
            )}
          </div>
        </motion.div>
      </div>
    </div>
  );
}

