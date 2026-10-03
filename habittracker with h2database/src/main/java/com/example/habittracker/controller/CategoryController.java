package com.example.habittracker.controller;

import com.example.habittracker.model.Category;
import com.example.habittracker.service.CategoryService;
import org.springframework.stereotype.Controller;
import org.springframework.ui.Model;
import org.springframework.web.bind.annotation.*;

@Controller
public class CategoryController {

    private final CategoryService categoryService;

    public CategoryController(CategoryService categoryService) {
        this.categoryService = categoryService;
    }

    // Display all categories
    @GetMapping("/categories")
    public String showCategories(Model model) {

        model.addAttribute("categories",
                categoryService.getAllCategories());

        return "categories";
    }

    // Show add category form
    @GetMapping("/category/new")
    public String showAddCategoryForm(Model model) {

        model.addAttribute("category", new Category());

        return "add-category";
    }

    // Save category
    @PostMapping("/category/save")
    public String saveCategory(@ModelAttribute Category category) {

        categoryService.saveCategory(category);

        return "redirect:/categories";
    }

    // Delete category
    @GetMapping("/category/delete/{id}")
    public String deleteCategory(@PathVariable Long id) {

        categoryService.deleteCategory(id);

        return "redirect:/categories";
    }
}