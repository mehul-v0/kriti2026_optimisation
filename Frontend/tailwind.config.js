/** @type {import('tailwindcss').Config} */
export default {
  content: [
    "./index.html",
    "./src/**/*.{js,ts,jsx,tsx}",
  ],
  theme: {
    extend: {
      colors: {
        primary: {
          DEFAULT: '#bce33c',
          dark: '#96b244',
          light: '#f07f7f',
          bright: '#cafb34',
          muted: '#8FAB6A',
        },
        button: {
          DEFAULT: '#bce33c',
          hover: '#bce33c',
        },
        dark: {
          DEFAULT: '#000000',
          900: '#000000',
          800: '#0a0a0a',
          750: '#121212',
          700: '#1a1a1a',
          600: '#242424',
          500: '#2e2e2e',
        },
        gray: {
          DEFAULT: '#808284',
          light: '#a0a0a0',
          dark: '#606060',
        },
      },
      boxShadow: {
        'float': '0 8px 30px rgba(0, 0, 0, 0.4), 0 4px 12px rgba(0, 0, 0, 0.3)',
        'float-lg': '0 20px 60px rgba(0, 0, 0, 0.5), 0 8px 20px rgba(0, 0, 0, 0.4)',
        'float-xl': '0 30px 80px rgba(0, 0, 0, 0.6), 0 12px 30px rgba(0, 0, 0, 0.5)',
        'glow': '0 0 20px rgba(217, 239, 146, 0.3), 0 0 40px rgba(217, 239, 146, 0.1)',
        'glow-lg': '0 0 30px rgba(217, 239, 146, 0.4), 0 0 60px rgba(217, 239, 146, 0.2)',
      },
      fontFamily: {
        sans: ['Inter', 'system-ui', 'sans-serif'],
        display: ['Outfit', 'Inter', 'sans-serif'],
      },
      animation: {
        'pulse-slow': 'pulse 3s cubic-bezier(0.4, 0, 0.6, 1) infinite',
        'float': 'float 6s ease-in-out infinite',
        'gradient': 'gradient 15s ease infinite',
      },
      keyframes: {
        float: {
          '0%, 100%': { transform: 'translateY(0)' },
          '50%': { transform: 'translateY(-20px)' },
        },
        gradient: {
          '0%, 100%': { backgroundPosition: '0% 50%' },
          '50%': { backgroundPosition: '100% 50%' },
        },
      },
      backgroundSize: {
        '300%': '300%',
      },
    },
  },
  plugins: [],
}
