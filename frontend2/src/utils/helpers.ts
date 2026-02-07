import { format } from 'date-fns';

export const formatCurrency = (amount: number): string => {
  return `₹${amount.toLocaleString('en-IN', { maximumFractionDigits: 0 })}`;
};

export const formatNumber = (num: number): string => {
  return num.toLocaleString('en-IN');
};

export const formatPercentage = (value: number): string => {
  return `${value.toFixed(1)}%`;
};

export const formatDate = (date: string | Date): string => {
  return format(new Date(date), 'PPp');
};

export const formatTime = (time: string): string => {
  return format(new Date(`2000-01-01T${time}`), 'h:mm a');
};

export const formatDuration = (minutes: number): string => {
  const hours = Math.floor(minutes / 60);
  const mins = Math.round(minutes % 60);
  if (hours > 0) {
    return `${hours}h ${mins}m`;
  }
  return `${mins}m`;
};

export const formatDistance = (km: number): string => {
  if (km < 1) {
    return `${Math.round(km * 1000)} m`;
  }
  return `${km.toFixed(1)} km`;
};

export const getPriorityColor = (priority: string): string => {
  switch (priority.toLowerCase()) {
    case 'high':
      return 'text-red-400 bg-red-500/20 border-red-500/30';
    case 'medium':
      return 'text-yellow-400 bg-yellow-500/20 border-yellow-500/30';
    case 'low':
      return 'text-green-400 bg-green-500/20 border-green-500/30';
    default:
      return 'text-gray-400 bg-gray-500/20 border-gray-500/30';
  }
};

export const getStatusColor = (status: string): string => {
  switch (status.toLowerCase()) {
    case 'satisfied':
    case 'completed':
    case 'assigned':
      return 'badge-success';
    case 'warning':
    case 'relaxed':
      return 'badge-warning';
    case 'violated':
    case 'failed':
    case 'error':
      return 'badge-error';
    default:
      return 'badge-info';
  }
};

export const generateId = (): string => {
  return `${Date.now()}-${Math.random().toString(36).substr(2, 9)}`;
};

export const downloadJSON = (data: any, filename: string) => {
  const blob = new Blob([JSON.stringify(data, null, 2)], { type: 'application/json' });
  const url = URL.createObjectURL(blob);
  const link = document.createElement('a');
  link.href = url;
  link.download = filename;
  document.body.appendChild(link);
  link.click();
  document.body.removeChild(link);
  URL.revokeObjectURL(url);
};

export const getRouteColors = (count: number): string[] => {
  const colors = [
    '#D9EF92', '#FFB6C1', '#87CEEB', '#98FB98', '#DDA0DD',
    '#F0E68C', '#87CEFA', '#FFA07A', '#B0E0E6', '#FFD700',
    '#AFEEEE', '#FFDAB9', '#E0BBE4', '#FFDEAD', '#C1FFC1',
  ];
  return colors.slice(0, count);
};

export const parseExcelDate = (serial: number): Date => {
  const utc_days = Math.floor(serial - 25569);
  const utc_value = utc_days * 86400;
  const date_info = new Date(utc_value * 1000);
  return date_info;
};

export const calculateTimeDifference = (start: string, end: string): number => {
  const startTime = new Date(`2000-01-01T${start}`);
  const endTime = new Date(`2000-01-01T${end}`);
  return (endTime.getTime() - startTime.getTime()) / (1000 * 60); // minutes
};
