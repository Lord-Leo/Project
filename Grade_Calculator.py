import tkinter as tk
from tkinter import messagebox

def calculate_grade():
    try:
        score = float(entry_score.get())
        if score<=100 and score >= 90:
            grade = "A+"
        elif score<=89 and score >= 80:
            grade = "A"
        elif score<=79 and score >= 70:
            grade = "B"
        elif score<=69 and score >= 60:
            grade = "C"
        elif score<=59 and score >= 40:
            grade = "D"
        else:
            grade = "F"
        messagebox.showinfo('Grade Info', f"You have obtained Grade {grade}")
    except ValueError:
        messagebox.showerror("Error", "Please enter a valid numeric score.")

app = tk.Tk()
app.title("Grade Calculator")
app.iconbitmap('F:/Programing/Python/calculator-3671700-3061901.ico')
app.config(bg='#e3eb07')
app.geometry('300x200')

label = tk.Label(app, text="Enter your score:", font=('times', 15), bg='#e3eb07')
label.pack(pady=10)

entry_score = tk.Entry(app)
entry_score.config(fg='#ed051c')
entry_score.pack()

button = tk.Button(app, text="Calculate Grade", command=calculate_grade)
button.config(bg='#11a4ed')
button.pack(pady=10)

app.mainloop()